// UpdateLauncher.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "pch.h"
//#include "resource.h"

#define _NOEXCEPT noexcept	// yaml-cpp0.6.2+vs2017 のビルドエラー回避
#include <yaml-cpp\yaml.h>
#ifdef _DEBUG
#pragma comment(lib,"../external/yaml-cpp-yaml-cpp-0.6.2build/Debug/libyaml-cppmtd.lib")
#else
#pragma comment(lib,"../external/yaml-cpp-yaml-cpp-0.6.2build/Release/libyaml-cppmt.lib")
#endif


#include "RLib\Base\Algorithm.h"
#include "RLib\Base\HttpGet.h"
#include "RLib\Base\File.h"
#include "RLib\Base\RemoteZip.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif



// 唯一のアプリケーション オブジェクトです。

CWinApp theApp;

namespace asio = boost::asio;
using namespace RLib;

namespace
{

void UpdateMain(asio::io_context &ioContext,const asio::yield_context &yield)
{
	std::error_code ec;

	const std::filesystem::path pathOwn = boost::dll::program_location().c_str();					// 自身(EXE)のファイルパス
	const std::filesystem::path pathYml = std::filesystem::path(pathOwn).replace_extension("yml");	// 設定ファイルパス

	struct CSettings				// 設定情報
	{
		std::filesystem::path	targetDir;
		std::string				remoteZip;
		std::filesystem::path	launchExe;
	}const settings = [&](){
			std::fstream st(pathYml.generic_wstring(),std::ios::in|std::ios::binary,_SH_DENYRD);
			if( st.fail() ){		// 成功？
				throw std::runtime_error(APPLOG_ERROR(R"(can't file open "%s")",pathYml.string()));
			}
			const YAML::Node node = YAML::Load(st);
			auto targetZip = node["targetDir"].as<std::string>("");
			auto remoteZip = node["remoteZip"].as<std::string>("");
			auto launchExe = node["launchExe"].as<std::string>("");

			// 置換処理
			for( auto &i : {&targetZip,&launchExe} ){
				std::string exeDir = pathOwn.parent_path().append("").generic_u8string();
				*i = std::regex_replace( *i, std::regex(R"(\$\(ExeDir\))",std::regex_constants::icase), exeDir);
			}

			return CSettings{std::filesystem::u8path(targetZip),remoteZip,std::filesystem::u8path(launchExe)};
		}();

	// 最後にEXE起動＆多重起動防止
	struct CCreateProcess{
		std::unique_ptr<boost::interprocess::windows_shared_memory>	m_upWsm;
		std::filesystem::path										m_pathApp;		// 起動対象

		CCreateProcess(const std::string &sMutexName){
			try{					// 多重起動抑止
				m_upWsm = std::make_unique<boost::interprocess::windows_shared_memory>(boost::interprocess::create_only, sMutexName.c_str(), boost::interprocess::read_write, 1);
			}catch(...){
				APPLOG_ERROR(R"(mutex error "%s")",sMutexName);
				throw;
			}
		}

		~CCreateProcess(){
			m_upWsm.reset();		// 多重起動抑止解除
			if( !m_pathApp.empty() ){
				STARTUPINFO si = {0,};
				PROCESS_INFORMATION pi = {0,};
				si.cb = sizeof(si);
				APPLOG_INFO( R"(CreateProcess "%s")",m_pathApp.string());
				if( !::CreateProcess(m_pathApp.wstring().c_str(), L"", 0, 0, FALSE, 0, 0, 0, &si, &pi) ){
					APPLOG_ERROR("CreateProcess error : %s",m_pathApp);
				}
				if( pi.hThread ) ::CloseHandle(pi.hThread);
				if( pi.hProcess ) ::CloseHandle(pi.hProcess);
			}
		}
	}createProcess(pathYml.generic_u8string());
	createProcess.m_pathApp = settings.launchExe;

	// Entryに対応するローカルのファイルパス取得関数
	auto fGetLocalPath = [&settings](const Zip::CCentralDirectoryEntry &entry){
			const std::filesystem::path path = std::filesystem::path(settings.targetDir)
				.append(entry.GetFilePath())
				.lexically_normal();
			struct{
				std::filesystem::path target;
				std::filesystem::path tmp;
				std::filesystem::path bak;
			}paths = { path, std::filesystem::path(path).concat(".tmp"), std::filesystem::path(path).concat(".bak") };
			return paths;
		};

	// ZIP セントラルディレクトリ情報取得
	const RLib::RemoteZip::CCentralDirectories cd = RLib::RemoteZip::GetCentralDirectories(ioContext,yield,settings.remoteZip);

	// アップデート対象選択
	struct CUncomp{
		Zip::CDecompressor	dec;		// 解凍処理
		std::ofstream			stTmp;		// 作成ファイル
	};
	std::map<std::shared_ptr<const Zip::CCentralDirectoryEntry>,std::shared_ptr<CUncomp>> mapUncomp = [&]{
			std::remove_const<decltype(mapUncomp)>::type mapUncomp;
			for( auto i : cd.entries ){
				if( ! [&i,&fGetLocalPath]{	// 違うファイル？
						auto &entry = *i;
						auto path = fGetLocalPath(entry).target;
						boost::iostreams::filtering_ostream	fo;
						fo.push( IoStream::CCrc32Sink() );
						boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> isfds( boost::filesystem::path(path),std::ios::in|std::ios::binary );
						fo << &isfds;
						fo.flush();
						auto info = fo.component<IoStream::CCrc32Sink>(0)->GetInfo();
						uint32_t crc32 = info.crc32.checksum();
						auto fileSize = info.count;
						if( entry.GetUncompressedSize()==fileSize && entry.m_fixed.common.crc32==crc32 ){
							return true;
						}
						return false;
					}()
				){
					mapUncomp.emplace(i,nullptr);				// アップデート対象にする
				}
			}
			return mapUncomp;
		}();

	{// tmp ファイルにダウンロード
		RLib::RemoteZip::DownloadFileData::CParameter param;
		param.sUrl = settings.remoteZip;
		param.centralDirectories = cd;
		for( auto &i : mapUncomp ) param.mapTarget[i.first] = 0;

		RLib::RemoteZip::DownloadFileData::Run(ioContext,yield,param,
			[&](const RLib::RemoteZip::DownloadFileData::CMapResult &mapResult){	// ダウンロード進捗処理
				for( auto &i : mapResult ){
					const auto spEntry = i.first;
					auto it = mapUncomp.find(spEntry);
					if( it == mapUncomp.end() ){
						throw std::runtime_error(APPLOG_ERROR("unknown entry : %s",spEntry->GetFilePath()));
					}
					auto &spUncomp = it->second;
					if( !spUncomp ){						// 開始？
						const auto paths = fGetLocalPath(*spEntry);
						std::filesystem::create_directories(paths.target.parent_path(),ec);	// ディレクトリがなければ作成。成否は気にしない
						spUncomp = std::shared_ptr<CUncomp>(new CUncomp{					// std::make_shared を使う方法がわからない
								Zip::CDecompressor(spEntry->m_fixed.common.compressionMethod),
								std::ofstream(paths.tmp,std::ios::out|std::ios::binary,_SH_DENYWR)	// ファイル作成
							});
						if( spUncomp->stTmp.fail() ){		// ファイル成功？
							throw std::runtime_error(APPLOG_ERROR(R"(can't file open "%s")",paths.tmp.string()));
						}
					}
					std::vector<char> v;
					std::copy(i.second.spData->begin(),i.second.spData->end(),std::back_inserter(v));
					i.second.spData->clear();
					spUncomp->dec.Push( v.data(), v.size() );												// 解凍
					auto vUncomp = spUncomp->dec.Consume();
					std::copy(vUncomp.begin(),vUncomp.end(),std::ostream_iterator<char>(spUncomp->stTmp));	// ファイルに追記
					APPLOG_INFO(R"(%8d/%8d "%s" )",spUncomp->dec.GetResult().nUncompCount, spEntry->GetUncompressedSize(), spEntry->GetFilePath() );
				}
				return true;
			}
		);
	}

	// ファイル置換の事前処理＆エラーチェック
	for(auto &i : mapUncomp){
		auto spUncomp = i.second;
		if( !spUncomp ){
			throw std::runtime_error(APPLOG_ERROR("not downloaded : %s",i.first->GetFilePath()));	// ダウンロード対象なのにダウンロードできてない
		}

		spUncomp->stTmp.close();				// ファイルクローズ

		const auto spEntry = i.first;
		const auto paths = fGetLocalPath(*spEntry);						// 対象ローカルファイルパス

		{// ファイル更新日時の設定。vs2017 では std::filesystem::last_write_time で日時を指定する方法がわからない
			auto tp = spEntry->m_fixed.common.GetModifiedDate();
			time_t tt = std::chrono::system_clock::to_time_t(tp);
			boost::filesystem::last_write_time( boost::filesystem::path(paths.tmp.generic_wstring()), tt );
		}

		auto &dec = spUncomp->dec;
		if( dec.GetResult().crc32.checksum() != spEntry->m_fixed.common.crc32 ){	// CRC チェック
			throw std::runtime_error(APPLOG_ERROR(R"(crc error "%s")",spEntry->GetFilePath()));
		}
		if( dec.GetResult().nUncompCount!=spEntry->GetUncompressedSize() ){			// ファイルサイズチェック
			throw std::runtime_error(APPLOG_ERROR(R"(file size error "%s")",spEntry->GetFilePath()));
		}

		// 元ファイルのアクセス権設定
		File::ACL::AddAuthenticatedUser(paths.target);					// 元ファイル(EXE)に Authenticated User を付与する
		File::ACL::AddAuthenticatedUser(paths.target.parent_path());	// 元ファイルがあるフォルダに Authenticated User を付与する

		std::filesystem::remove_all(paths.bak,ec);						// bak があれば削除

		if( std::filesystem::exists(paths.bak) ){
			throw std::runtime_error(APPLOG_ERROR(R"(can't remove bak file "%s")",spEntry->GetFilePath()));	// bak ファイルが削除できない
		}
	}

	// ファイル置換
	for(auto &i : mapUncomp){
		const auto paths = fGetLocalPath(*i.first);			// 対象ローカルファイルパス
		std::filesystem::rename(paths.target,paths.bak,ec);	// 元のファイルをbakにリネーム
		std::filesystem::rename(paths.tmp,paths.target,ec);	// 新ファイル(.tmp)に差し替える
	}

	// 自分自身が置換されていたら自分自身を再起動対象に
	for(auto &i : mapUncomp){
		const auto paths = fGetLocalPath(*i.first);
		if( std::filesystem::equivalent(pathOwn,paths.target,ec) ){		// 自分自身？
			createProcess.m_pathApp = pathOwn;
			break;
		}
	}

}


}

int main(int argc, char** argv)
{
	// ログ初期化
	auto spAppLog = CAppLog::Initialize( std::filesystem::path(boost::dll::program_location().replace_extension("log").generic_wstring()).generic_u8string() );

	int nRetCode = 0;

	{// MFC の初期化
		HMODULE hModule = ::GetModuleHandle(nullptr);
		if (hModule != nullptr){
			// MFC を初期化して、エラーの場合は結果を印刷します。
			if (!::AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)){
				// TODO: 必要に応じてエラー コードを変更してください。
				wprintf(L"致命的なエラー: MFC の初期化ができませんでした。\n");
				nRetCode = 1;
			}else{
				// TODO: アプリケーションの動作を記述するコードをここに挿入してください。
			}
		}else{
			// TODO: 必要に応じてエラー コードを変更してください。
			wprintf(L"致命的なエラー: GetModuleHandle が失敗しました\n");
			nRetCode = 1;
		}
		if( nRetCode != 0 ){
			return nRetCode;
		}
	}

	// オプションの設計
	namespace po = boost::program_options;
	po::options_description desc("RemoteZipSync [options] source destination");
	desc.add_options()
		("help,h",											"show help")
		("version,v",										"show version");
//		("target",	po::value<std::vector<std::string>>(),	"target");

	//const auto positional = po::positional_options_description()
	//	.add("target", 2);

	po::variables_map vm;
	try{
		po::store(
			po::command_line_parser(argc, argv)
				.options(desc)
//				.positional(positional)
				.run()
			,vm);
		po::notify(vm);
	}catch(std::exception& e){
		std::cout << e.what() << std::endl;
		std::cout << desc << std::endl;				// ヘルプ表示
		return 0;
	}

	if( vm.count("help") ){
		std::cout << desc << std::endl;				// ヘルプ表示
		return 0;
	}

	if( vm.count("version") ){
		std::cout << "1.0.0" << std::endl;
		return 0;
	}

	static std::atomic_flag bAbort = ATOMIC_FLAG_INIT;
	for( auto i : {
#ifdef _MSC_VER
			SIGINT,SIGBREAK,
#else
			SIGINT,SIGHUP,SIGTERM
#endif
		}
	){
		std::signal( i, [](int n){
				bAbort.clear(std::memory_order_release);
			});
	}

	nRetCode = 1;
	asio::io_context ioContext;
	asio::spawn( ioContext,
		[&ioContext,&nRetCode](asio::yield_context yield){
			try{
				UpdateMain(ioContext,yield);
				nRetCode = 0;
			}catch(boost::system::error_code ec){
				APPLOG_ERROR("exception error_code : %s",ec.message());
			}catch(std::exception &e){
				APPLOG_ERROR("exception std::exception : %s",e.what());
			}catch(boost::coroutines::detail::forced_unwind&){
				APPLOG_ERROR("exception boost::coroutines::detail::forced_unwind");
			}catch(...){
				auto ep = std::current_exception();
				APPLOG_ERROR("exception unknown");
			}
		});
	ioContext.run();
	return nRetCode;

#if 0
	auto f = std::async(std::launch::async, [](){
			try{
				asio::io_context ioContext;
				asio::spawn( ioContext,
					[&ioContext](asio::yield_context yield){
						UpdateMain(ioContext,yield);
					});
				ioContext.run();
			}catch(...){
				auto ep = std::current_exception();
			}
			return 0;
		});
	nRetCode = f.get();
	return nRetCode;
#endif
}

