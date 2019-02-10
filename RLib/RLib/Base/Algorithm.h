#pragma once

namespace RLib
{

namespace Time
{
	// std::chrono::steady_clock::time_point を std::chrono::system_clock::time_point に変換
	inline std::chrono::system_clock::time_point SteadyClockToSystemClock( std::chrono::steady_clock::time_point t ){
		return std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>( t - std::chrono::steady_clock::now() );
	}

	// 日時を文字列に変換
	// "%Y-%m-%dT%H:%M:%S%z"-> "2018-05-28T15:49:43+0900"
	// "%FT%T"				-> "2018-05-28T15:49:43"
	// "%Y%m%dT%H%M%S"		-> "20180528T154943"
	inline std::string TimePointFormat(const std::chrono::system_clock::time_point &tp,std::string sFormat="%Y-%m-%dT%H:%M:%S%z"){
		time_t t = std::chrono::system_clock::to_time_t(tp);


#ifdef _MSC_VER
		tm lt_;
		errno_t error = ::localtime_s(&lt_, &t);
		const tm* lt = &lt_;
#else

//#ifdef _MSC_VER
//__pragma(warning(push))
//__pragma(warning(disable:4996))
//#endif
		const tm* lt = std::localtime(&t);
//#ifdef _MSC_VER
//__pragma(warning(pop))
//#endif

#endif

		std::ostringstream ss;
		ss << std::put_time(lt, sFormat.c_str());
		return ss.str();
	}

	// 秒数以下の表現を取得
	// 例 ".323277950287"
	inline std::string TimePointDecimalFormat(const std::chrono::system_clock::time_point &tp){
		const auto full = std::chrono::duration_cast< std::chrono::duration<long double> >( tp.time_since_epoch() );
		const auto sec  = std::chrono::duration_cast< std::chrono::seconds >( tp.time_since_epoch() );
		const auto decimal = ( full - sec ).count();
		std::ostringstream ss;
		ss << std::setprecision(12) << std::setiosflags(std::ios::fixed) << decimal;
		return ss.str().substr(1);
	}

	// system_clockを表示する関数
	inline void print_time(const std::chrono::system_clock::time_point &tp){
		auto s = TimePointFormat(tp,"%Y-%m-%d %H:%M:%S(d)%z");
		boost::algorithm::replace_first(s,"(d)",TimePointDecimalFormat(tp));
		//RTRACE("\n%s",s);
	}
}


namespace IoStream
{
	// source の土台
	class CSource : public boost::iostreams::source{
		std::function<std::streamsize (char*,std::streamsize)>	m_func;
	public:
		template <typename Func> CSource(Func func)
			:m_func(func)
		{}
		std::streamsize read( char *p, std::streamsize n )
		{
			return m_func(p,n);
		}
	};

	// sink の土台
	class CSink : public boost::iostreams::sink{
		std::function<std::streamsize (const char*,std::streamsize)>	m_func;
	public:
		template <typename Func> CSink(Func func)
			:m_func(func)
		{}
		std::streamsize write(const char *p, std::streamsize n)
		{
			return m_func(p,n);
		}
	};


	class CCrc32Sink : public boost::iostreams::sink
	{
	public:
		struct CInfo
		{
			boost::crc_32_type	crc32;
			uint64_t			count = 0;
		}m_info;

		const CInfo& GetInfo()const{
			return m_info;
		}

		std::streamsize write(const char *p, std::streamsize size){
			m_info.count += size;
			m_info.crc32.process_bytes( p, static_cast<size_t>(size) );
			return size;
		}
	};

	class base64_encoder : public boost::iostreams::output_filter{
		char buf[3];
		int  index;
		static constexpr char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";		// 6bit→文字 変換テーブル
	public:
		base64_encoder()
			:index(0)
		{}

		template<typename Sink> bool put( Sink& snk, int c ){
			buf[index++] = c;	// バッファにためる
			if( index == 3 ){	// 3文字たまったら6bitに区切り出力
				boost::iostreams::put( snk, tbl[(buf[0]>>2)&0x3f] );
				boost::iostreams::put( snk, tbl[(buf[0]<<4)&0x30 | (buf[1]>>4)&0x0f] );
				boost::iostreams::put( snk, tbl[(buf[1]<<2)&0x3c | (buf[2]>>6)&0x03] );
				boost::iostreams::put( snk, tbl[(buf[2])   &0x3f] );
				index = 0;
			}
			return true;
		}

		template<typename Sink> void close( Sink& snk ){
			if( index > 0 ){	// 余りあり？
				for(int j=index; j<3; ++j) buf[j] = 0;
				boost::iostreams::put( snk, tbl[(buf[0]>>2)&0x3f] );
				boost::iostreams::put( snk, tbl[(buf[0]<<4)&0x30 | (buf[1]>>4)&0x0f] );
				boost::iostreams::put( snk, index<=1 ? '=' : tbl[(buf[1]<<2)&0x3c | (buf[2]>>6)&0x03] );
				boost::iostreams::put( snk, index<=2 ? '=' : tbl[(buf[2])   &0x3f] );
			}
		}
	};

}

}
