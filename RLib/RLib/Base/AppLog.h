#pragma once

namespace RLib
{

class CAppLog
	:private boost::noncopyable
{
	CAppLog(){}
public:
	struct Inner{
		static std::string Format(const boost::format &format) noexcept{
			return format.str();
		}
		static std::string Format(const boost::wformat &format) noexcept{
			const std::wstring ws = format.str();
//			return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes(reinterpret_cast<const char16_t*>(ws.c_str()));	// UTF16 → UTF8
			return std::wstring_convert<std::codecvt_utf8_utf16<uint16_t>,uint16_t>().to_bytes(reinterpret_cast<const uint16_t*>(ws.c_str()));	// UTF16 → UTF8
		}
#ifdef _MSC_VER
		template <class CharT,class... Tail> static std::string Format(boost::basic_format<CharT> &format,const ATL::CStringT<CharT,StrTraitMFC_DLL<CharT>>& head,Tail&&... tail) noexcept{
			return Format( format % std::basic_string<CharT>(head), tail... );
		}
		template <class CharT,class... Tail> static std::string Format(boost::basic_format<CharT> &format,ATL::CStringT<CharT,StrTraitMFC_DLL<CharT>>& head,Tail&&... tail) noexcept{
			return Format( format % std::basic_string<CharT>(head), tail... );
		}
#endif
		template <class CharT,class Head,class... Tail> static std::string Format(boost::basic_format<CharT> &format,Head&& head, Tail&&... tail) noexcept{
			return Format( format % head, tail... );
		}
		template <class CharT,class... Args> static std::string Format(const CharT *lpszFormat,Args&&... args) noexcept{
			boost::basic_format<CharT> format;
			format.exceptions( boost::io::no_error_bits );	// 例外を発生させない
			format.parse(lpszFormat);
			return Inner::Format(format,args...);
		}
		template <class CharT,class... Args> static std::string Format(const std::basic_string<CharT> &s,Args&&... args) noexcept{
			return Format<CharT>(s.c_str(),args...);
		}
	};

	~CAppLog(){
		boost::log::core::get()->remove_all_sinks();
	}

	// ログシステムの初期化
	// ・最初にコールして戻り値を保持すべし。削除時に remove_all_sinks() される。
	// ・例: auto spAppLog = CAppLog::Initialize( std::filesystem::path(boost::dll::program_location().replace_extension("log").generic_wstring()).generic_u8string() );
	static std::shared_ptr<CAppLog> Initialize(const std::filesystem::path &pathFolder,const std::string &sFile="%Y%m%d_%5N.log"){
		static std::recursive_mutex mutex;
		std::lock_guard<std::recursive_mutex> lock(mutex);
		static std::weak_ptr<CAppLog> wpAppLog;
		auto spAppLog = wpAppLog.lock();
		if( spAppLog ) return spAppLog;
		wpAppLog = spAppLog = std::shared_ptr<CAppLog>(new CAppLog);

		namespace log = boost::log;
		static const auto formatTimeStamp = log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S");
		static const auto formatThreadId = log::expressions::attr<log::attributes::current_thread_id::value_type>("ThreadID");

#ifdef _MSC_VER
		{// Visual Studio の 出力ウインドウ
			struct CDebugOutputBackend : public log::sinks::basic_formatted_sink_backend<char,log::sinks::concurrent_feeding>{	// UTF8 を出力するためのカスタム sink
				void consume(log::record_view const& rec, string_type const& formatted_message){
					::OutputDebugString( CA2T(formatted_message.c_str(),CP_UTF8) );	// UTF8 → CString
				}
			};
			auto spBackend = boost::make_shared<CDebugOutputBackend>();
			auto spSink = boost::make_shared<log::sinks::synchronous_sink<CDebugOutputBackend>>(spBackend);
			spSink->set_formatter(
					log::expressions::format("\n%1%\t%2%\t%3%\t%4%")			// \n で改行させる
					% formatTimeStamp
					% formatThreadId
					% log::trivial::severity
					% log::expressions::message
				);
			log::core::get()->add_sink(spSink);
		}
#endif	//#ifdef _MSC_VER

		{ // std::clog
			auto spBackend = boost::make_shared<log::sinks::text_ostream_backend>();
			spBackend->add_stream(boost::shared_ptr<std::ostream>( &std::clog, boost::null_deleter()) );
			spBackend->auto_flush(true);
			auto spSink = boost::make_shared<log::sinks::synchronous_sink<log::sinks::text_ostream_backend>>(spBackend);
			spSink->set_formatter(
					log::expressions::format("%1%\t%2%\t%3%\t%4%")
					% formatTimeStamp
					% formatThreadId
					% log::trivial::severity
					% log::expressions::message
				);
			log::core::get()->add_sink(spSink);
		}

		{// file
			namespace keywords = log::keywords;
			log::add_file_log(
				keywords::auto_flush = true,					// 出力のたびに flush する。
				keywords::open_mode = std::ios::app,
				keywords::target = pathFolder,
				keywords::file_name = std::filesystem::path(pathFolder).append(sFile),
				keywords::time_based_rotation = log::sinks::file::rotation_at_time_point(0, 0, 0),
				keywords::max_size = 256 * 1024 * 1024,			// フォルダの最大サイズ。これを超えるとファイルが削除される。
				keywords::rotation_size = 16 * 1024 * 1024,		// 1ファイルの最大サイズ。これを超えると別ファイルになる。file_name に ".._%5N.log" とか書くべし
				keywords::max_files = 256,						// 最大ファイル数
				// keywords::min_free_space = 1024 * 1024 * 1024,	// ドライブの最小空き容量
				keywords::format =
					log::expressions::format("%1%\t%2%\t%3%\t%4%")
					% formatTimeStamp
					% formatThreadId
					% log::trivial::severity
					% log::expressions::message
			);
		}

		log::add_common_attributes();
#ifdef _DEBUG
		// do nothing
#else
		// log::core::get()->set_filter( log::trivial::severity >= log::trivial::info );
#endif

		return spAppLog;
	}

	template <class CharT,class... Args> static std::string Put(boost::log::trivial::severity_level level,const CharT *lpszFormat,Args&&... args) noexcept{
		const std::string s = Inner::Format(lpszFormat,args...);
		BOOST_LOG_STREAM_WITH_PARAMS(boost::log::trivial::logger::get(),(boost::log::keywords::severity = level)) << s;
		return s;
	}

};

}


// ファイル名:行番号 の形式で出力
#define APPLOG_DEBUG(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::debug,	"%s:%d\t%s", std::filesystem::path(__FILE__).filename(),__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_INFO(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::info,	"%s:%d\t%s", std::filesystem::path(__FILE__).filename(),__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_WARNING(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::warning,"%s:%d\t%s", std::filesystem::path(__FILE__).filename(),__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_ERROR(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::error,	"%s:%d\t%s", std::filesystem::path(__FILE__).filename(),__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_FATAL(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::fatal,	"%s:%d\t%s", std::filesystem::path(__FILE__).filename(),__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))

// 関数名:行番号 の形式で出力（無名関数だと長ったらしくなるのでオススメしない）
#define APPLOG_FUNC_DEBUG(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::debug,	"%s:%d\t%s", __FUNCTION__,__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_FUNC_INFO(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::info,	"%s:%d\t%s", __FUNCTION__,__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_FUNC_WARNING(fmt,...)RLib::CAppLog::Put(boost::log::trivial::warning,"%s:%d\t%s", __FUNCTION__,__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_FUNC_ERROR(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::error,	"%s:%d\t%s", __FUNCTION__,__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))
#define APPLOG_FUNC_FATAL(fmt,...)	RLib::CAppLog::Put(boost::log::trivial::fatal,	"%s:%d\t%s", __FUNCTION__,__LINE__, RLib::CAppLog::Inner::Format(fmt,__VA_ARGS__))

