#include "RLib/RLibPrecompile.h"
#include "./RemoteZip.h"

#include "RLib/Base/AppLog.h"
#include "RLib/Base/HttpGet.h"
#include "RLib/Base/HttpGetMultiPart.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

using namespace RLib;

namespace beast = boost::beast;
namespace http = boost::beast::http;

RemoteZip::CCentralDirectories RemoteZip::GetCentralDirectories(asio::io_context &ioContext,const asio::yield_context &yield,const std::string &sUrl)
{
	auto fHttpGet = [&ioContext,&yield,sUrl](const Http::FuncPreRequest &fPreRequest,const Http::FuncProgress &fProgress=Http::FuncProgress()){
			const Http::CTarget target = Http::CTarget::ParseUri(sUrl);
			auto r = Http::Get(ioContext,yield,target
				,[&](http::request<http::string_body> &req){
					fPreRequest(req);
					auto i = req.find( http::field::range );
					APPLOG_INFO( R"(HTTP GET "%s" range:"%s")",sUrl, i!=req.end() ? i->value().to_string() : "" );
				}
				,fProgress);
			const auto nHttpStatus = r.message.result_int();
			if( r.ec ){
				throw std::runtime_error(APPLOG_ERROR("boost::system::error_code %d:%s",r.ec.value(),String::SystemLocaleToUtf8(r.ec.message())));
			}
			switch( nHttpStatus ){
			case 200:
			case 206:
				APPLOG_INFO( R"(http status:%d)",nHttpStatus);
				break;
			default:
				throw std::runtime_error(APPLOG_ERROR("http status %d",nHttpStatus));
			}
			return r;
		};

	CCentralDirectories result;

	// ZIP�Z���g�����f�B���N�g���I�[���R�[�h(EOCD)���擾
	result.spEndOfCentralDirectory = [&]{
			auto result = fHttpGet(
				[](http::request<http::string_body> &req){
					req.set( http::field::range, String::Format("bytes=-%d",sizeof(Zip::ZipEndOfCentralDirectoryRecord)) );
				});
			const std::vector<char> v = Http::Utility::ConsumeBody(result.message);
			if( v.size() != sizeof(Zip::ZipEndOfCentralDirectoryRecord) ){
				throw std::runtime_error(APPLOG_ERROR("error Loaded filesize."));
			}
			const Zip::ZipEndOfCentralDirectoryRecord *p = reinterpret_cast<const Zip::ZipEndOfCentralDirectoryRecord*>(&v[0]);
			// �V�O�l�`���`�F�b�N "PK" "\5" "\6"
			if( ::memcmp(&p->endOfCentralDirSignature,"PK""\5""\6",sizeof(Zip::ZipEndOfCentralDirectoryRecord::endOfCentralDirSignature)) != 0 ){
				throw std::runtime_error(APPLOG_ERROR("error signature."));
			}
			return std::make_shared<Zip::ZipEndOfCentralDirectoryRecord>(*p);
		}();

	// �Z���g�����f�B���N�g���J�n�ʒu����t�@�C�������܂Ŏ擾
	const std::vector<char> v = [&]{
			auto r = fHttpGet(
				[&](http::request<http::string_body> &req){
					req.set( http::field::range, String::Format("bytes=%llu-",result.spEndOfCentralDirectory->offsetOfTheStartOfCentralDirectory) );
				});
			return Http::Utility::ConsumeBody(r.message);
		}();

	{// �Z���g�����f�B���N�g���G���g�����X�g�擾
		const auto pair = Zip::ParseCentralDirectory(result.spEndOfCentralDirectory->totalNumberOfEntries,std::istringstream(std::string(&v[0],v.size())));
		if( !pair.first ){		// error?
			throw std::runtime_error(APPLOG_ERROR("error parse central directory."));
		}
		for( auto &i : pair.second ){
//			result.vEntry.push_back(std::make_shared<Zip::CCentralDirectoryEntry>(i));
			result.entries.insert(std::make_shared<Zip::CCentralDirectoryEntry>(i));
		}
	}

/*
	{// �Z���g�����f�B���N�g���ȍ~�̃n�b�V���l(zip_hash)
		boost::uuids::detail::md5 md5;
		md5.process_bytes( &v[0], v.size() );
		boost::uuids::detail::md5::digest_type digest;
		md5.get_digest( digest );
		const uint8_t *p = reinterpret_cast<uint8_t*>( digest );
		assert(sizeof(digest)==result.zipHash.size());
		std::copy( p, p+sizeof(digest), result.zipHash.begin() );
	}
*/

	return result;
}



RemoteZip::DownloadFileData::CMapResult RemoteZip::DownloadFileData::Run(asio::io_context &ioContext,const asio::yield_context &yield,const CParameter &parameter,const std::function<bool (const CMapResult&)> &fProgress)
{
	// ��ԕێ��N���X
	struct CState
	{
		const std::shared_ptr<const Zip::CCentralDirectoryEntry>	spEntry;
		const std::streamoff										nDownloadedSize;	// �_�E�����[�h�σT�C�Y(����ȑO�̃f�[�^�̓_�E�����[�h�ΏۊO)
		const std::streamoff										nAreaSize;			// ���̃G���g���̃T�C�Y(���G���g���܂ł̃o�C�g��)
		std::unique_ptr<Zip::CLocalFileHeader::CFixed>			upHeaderFixed;
		uint64_t													nDownloadedCount = 0;
		std::shared_ptr<std::deque<char>>							spData = std::make_shared<std::deque<char>>();	// �_�E�����[�h�����f�[�^

		struct CLess{
			typedef void is_transparent;
			bool operator()(const std::shared_ptr<CState> &a, const std::shared_ptr<CState> &b)const{
				return a->spEntry->GetPostionLocalHeader() < b->spEntry->GetPostionLocalHeader();
			}
			bool operator()(std::streamoff a, const std::shared_ptr<CState> &b)const{
				return a < b->spEntry->GetPostionLocalHeader();
			}
			bool operator()(const std::shared_ptr<CState> &a, std::streamoff b)const{
				return a->spEntry->GetPostionLocalHeader() < b;
			}
		};
	};

	// CCentralDirectoryEntry(���[�J���t�@�C���w�b�_�̈ʒu)���L�[�ɂ�����ԕێ��N���X�� map
	const std::set<std::shared_ptr<CState>,CState::CLess> mapDetail = [&parameter]{
			std::remove_const<decltype(mapDetail)>::type result;

			auto iEnd = parameter.centralDirectories.entries.end();
			for( auto i=parameter.centralDirectories.entries.begin(); i!=iEnd; i++){
				auto spEntry = *i;
				const std::streamoff nDownloadedSize = [&]{		// �_�E�����[�h�σT�C�Y�B(�_�E�����[�h�ΏۊO�Ȃ�ő�l)
						auto i = parameter.mapTarget.find(spEntry);
						return i!=parameter.mapTarget.end() ? i->second : (std::numeric_limits<std::streamoff>::max)();
					}();
				const std::streamoff areaSize = [&](){			// ���̃G���g���̃T�C�Y
						auto iNext = i;
						iNext++;
						const std::streamoff nextOffset = iNext!=iEnd ? (*iNext)->GetPostionLocalHeader() : parameter.centralDirectories.spEndOfCentralDirectory->offsetOfTheStartOfCentralDirectory;	// ���G���g���̈ʒu
						return nextOffset - (*i)->GetPostionLocalHeader();
					}();
				result.insert( std::make_shared<CState>(CState{spEntry,nDownloadedSize,areaSize}) );
			}
			return result;
		}();

	// range �t���Ń_�E�����[�h����֐�
	auto fHttpGetRange = [&ioContext,&yield,&parameter](const std::map<uint64_t,uint64_t> &mapRange,const std::function<bool (const std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*>&)> &fProgress=nullptr){
			Http::GetMultiPart::CParameter param;
			param.sUrl = parameter.sUrl;
			param.mapRange = mapRange;
			param.nMaxRanges = parameter.nMaxRanges;
			auto r = Http::GetMultiPart::Run( ioContext, yield, param, fProgress );
			for( auto &msg : r.vMessage ){
				if( msg.ec ){
					throw std::runtime_error(APPLOG_ERROR("error system::error_code %d:%s",msg.ec.value(),String::SystemLocaleToUtf8(msg.ec.message())));
				}
				const auto nHttpStatus = msg.httpMessage.result_int();
				switch( nHttpStatus ){
				case 200:
				case 206:
					APPLOG_INFO( R"(http status:%d)",nHttpStatus,msg.sRange);
					break;
				default:
					throw std::runtime_error(APPLOG_ERROR(R"(http status:%d range:"%s")",nHttpStatus,msg.sRange));
				}
			}
			return r;
		};

	// �_�E�����[�h�i���n���h��
	auto fDownloadProgress = [&fProgress](const std::map<uint64_t,std::shared_ptr<CState>> &mapState,const std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*> &mapProgress){
			CMapResult vProgress;
			for( auto &iProgress : mapProgress ){
				if( auto it = mapState.find(iProgress.first.first); it==mapState.end() ){
					throw std::runtime_error(APPLOG_ERROR("unknown range %d,%d",iProgress.first.first,iProgress.first.second));
				}else{
					CState &state = *(it->second);
					auto &vBody = *iProgress.second;

					if( !state.upHeaderFixed ){															// ���[�J���t�@�C���w�b�_�͖��ǂݍ��݁H
						std::istringstream is(std::string(vBody.data(),vBody.size()));
						auto upHeader = Zip::CLocalFileHeader::ReadFromStream(is);
						if( !upHeader ) continue;														// ���[�J���t�@�C���w�b�_�����[�h���I���Ă��Ȃ��H
						vBody.erase( vBody.begin(), vBody.begin()+upHeader->m_fixed.GetHeaderSize() );	// ���[�J���t�@�C���w�b�_�̕����o�b�t�@����폜
						state.upHeaderFixed = std::make_unique<Zip::CLocalFileHeader::CFixed>(upHeader->m_fixed);
					}

					if( vBody.empty() ) continue;

					const std::streamoff nRemain = state.spEntry->GetCompressedSize() - ( state.nDownloadedSize + state.nDownloadedCount );	// �c��T�C�Y
					if( nRemain <= state.spEntry->GetCompressedSize() ){
						if( vBody.size() > nRemain ){				// �����̗]�v�ȃf�[�^�͖���
							vBody.resize(static_cast<size_t>(nRemain));
						}
						state.spData->insert( state.spData->end(), vBody.begin(), vBody.end() );
						state.nDownloadedCount += vBody.size();

						CEntryInfo &ei = vProgress[state.spEntry];
						ei.nDownloadedSize = state.nDownloadedSize + state.nDownloadedCount;
						ei.spData = state.spData;
					}
					vBody.clear();		// ����
				}
			}
			if( fProgress && !vProgress.empty() ){
				return fProgress(vProgress);
			}
			return true;
		};

	{// �����(nDownloadedSize��1�ȏ�)�Ŗ������̃t�@�C�����������[�h

		// �Ώۂ̃��[�J���t�@�C���w�b�_���_�E�����[�h  map<���[�J���t�@�C���w�b�_�̈ʒu,CHeader>
		const std::set<std::shared_ptr<CState>,CState::CLess> mapTarget = [&]{
				std::remove_const<decltype(mapTarget)>::type result;
				const auto r = fHttpGetRange(
					[&mapDetail]{						// �Ώۂ�I��
						std::map<uint64_t,uint64_t> m;	// �Ώۂ̃��[�J���t�@�C���w�b�_�̈ʒu,�T�C�Y
						for( const auto &i : mapDetail ){
							auto const &state = *i;
							if( state.nDownloadedSize>0 && state.nDownloadedSize<state.spEntry->GetCompressedSize() ){	// �ΏہH
//							if(                            state.nDownloadedSize<state.spEntry->GetCompressedSize() ){	// �J���p
								m[state.spEntry->GetPostionLocalHeader()] = sizeof(Zip::CLocalFileHeader::CFixed);
							}
						}
						return m;
					}());
				// ���[�J���t�@�C���w�b�_�Ɋ��蓖��
				for( auto &i : r.mapRanges ){
					auto &vBody = i.second;
					if( vBody.empty() ) continue;		// failsafe
					if( auto upFixed = Zip::CLocalFileHeader::CFixed::ReadFromStream( std::istringstream(std::string(vBody.data(),vBody.size())) ) ){
						auto it = mapDetail.find(i.first.first);	// �Ώۂ��擾
						if( it == mapDetail.end() ){
							throw std::runtime_error(APPLOG_ERROR("unknown postiion %d,%d",i.first.first,i.first.second));
						}
						(*it)->upHeaderFixed = std::move(upFixed);
						result.insert(*it);
					}else{
						throw std::runtime_error(APPLOG_ERROR("unknown LocalFileHeader %d,%d",i.first.first,i.first.second));
					}
				}
				return result;
			}();

		// �_�E�����[�h�J�n�ʒu�A�T�C�Y���擾
		auto fGetDataRange = [](CState &state)->std::pair<uint64_t,uint64_t>{
				auto pos = state.spEntry->GetPostionLocalHeader() + state.upHeaderFixed->GetHeaderSize();	// ���k�t�@�C���̐擪�ʒu
				auto begin = pos + state.nDownloadedSize;													// �_�E�����[�h�J�n�ʒu(�_�E�����[�h�ς𑫂����ʒu)
				auto size = pos + state.spEntry->GetCompressedSize() - begin;								// �_�E�����[�h���ׂ��T�C�Y
				return {begin,size};
			};

		// �f�[�^�_�E�����[�h�p�� range to state �e�[�u��  map<�f�[�^�_�E�����[�h�J�n�ʒu,CState>
		const std::map<uint64_t,std::shared_ptr<CState>> mapState = [&]{
				std::remove_const<decltype(mapState)>::type result;
				for( auto &i : mapTarget ){
					auto pos = fGetDataRange(*i);
					result[pos.first] = i;
				}
				return result;
			}();

		// ���k�t�@�C�����̂��_�E�����[�h
		fHttpGetRange(
			[&]{
				std::map<uint64_t,uint64_t> m;
				for( auto &i : mapState ){
					m[i.first] =  fGetDataRange(*i.second).second;		// [�ʒu]=�T�C�Y
				}
				return m;
			}(),
			std::bind(fDownloadProgress,mapState,std::placeholders::_1));
	}

	{// ������(nDownloadedSize��0)�Ŗ������̃t�@�C�����������[�h

		// �f�[�^�_�E�����[�h�p�� range to state �e�[�u��  map<�f�[�^�_�E�����[�h�J�n�ʒu,CState>
		const std::map<uint64_t,std::shared_ptr<CState>> mapState = [&mapDetail]{
				std::remove_const<decltype(mapState)>::type result;
				for( auto &i : mapDetail ){
					if( i->nDownloadedSize==0 && i->spEntry->GetCompressedSize()>0 ){	// �ΏہH
						result[i->spEntry->GetPostionLocalHeader()] = i;
					}
				}
				return result;
			}();

		fHttpGetRange(
			[&mapState]{
				// ���[�J���t�@�C���w�b�_�ƃf�[�^����x�Ƀ_�E�����[�h
				std::map<uint64_t,uint64_t>	mapRange;
				for( const auto &i : mapState ){
					const auto &state = *i.second;
					mapRange[state.spEntry->GetPostionLocalHeader()] = state.nAreaSize;
				}
				return mapRange;
			}(),
			std::bind(fDownloadProgress,mapState,std::placeholders::_1));
	}

	CMapResult result;
	for(auto &i : parameter.mapTarget ){
		auto it = mapDetail.find(i.first->GetPostionLocalHeader());
		if( it != mapDetail.end() ){
			auto &state = **it;
			auto &s = result[state.spEntry];
			s.nDownloadedSize = state.nDownloadedSize + state.nDownloadedCount;
			s.spData = state.spData;
		}
	}

	return result;
}
