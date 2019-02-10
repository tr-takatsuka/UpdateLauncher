#pragma once

namespace RLib
{

namespace String
{

// Format

template <class CharT> std::basic_string<CharT> Format(const boost::basic_format<CharT> &format)
{
	return format.str();
}

#ifdef _MSC_VER
template <class CharT,class... Tail> std::basic_string<CharT> Format(boost::basic_format<CharT> &format,const ATL::CStringT<CharT,StrTraitMFC_DLL<CharT>>& head,Tail&&... tail)
{
	return Format<CharT>( format % std::basic_string<CharT>(head), tail... );
}
template <class CharT,class... Tail> std::basic_string<CharT> Format(boost::basic_format<CharT> &format,ATL::CStringT<CharT,StrTraitMFC_DLL<CharT>>& head,Tail&&... tail)
{
	return Format<CharT>( format % std::basic_string<CharT>(head), tail... );
}
#endif

template <class CharT,class Head,class... Tail> std::basic_string<CharT> Format(boost::basic_format<CharT> &format,Head&& head,Tail&&... tail)
{
	return Format<CharT>( format % head, tail... );
}

template <class CharT,class... Args> std::basic_string<CharT> Format(const CharT *lpszFormat,Args&&... args)
{
	boost::basic_format<CharT> format;
	format.exceptions( boost::io::no_error_bits );	// 例外を発生させない
	format.parse(lpszFormat);
	return Format<CharT>(format,args...);
}

template <class CharT,class... Args> std::basic_string<CharT> Format(const std::basic_string<CharT> &s,Args&&... args)
{
	return Format<CharT>(s.c_str(),args...);
}


// system local to UTF8
inline std::string SystemLocaleToUtf8(const std::string &localeStr){
    static const std::locale loc = []{
            boost::locale::generator g;
            g.locale_cache_enabled(true);
            return g(boost::locale::util::get_system_locale());
        }();
    return boost::locale::conv::to_utf<char>(localeStr.c_str(),loc);
}

// MultiBute to UTF8
inline std::string AtoUtf8(const char *psz)
{
#ifdef _MSC_VER
	std::string s( CT2A(CString(psz),CP_UTF8) );
	return s;
#else
	assert(false);
	// linux版は未実装
#endif
}

#ifdef _MSC_VER

// UTF8 → CString
inline CString Utf8toT(const char *psz)
{
	CString s( CA2T(psz,CP_UTF8) );	// UTF8 → CString
	return s;
}

// CString → UTF8
inline std::string TtoUtf8(LPCTSTR lpsz)
{
	return std::string( CT2A( lpsz, CP_UTF8 ));
}

#endif


// HEX文字列をバイナリにする
// 例	auto v = String::HexStringToBinary("0189ABEFabef");
template <class CharT> std::optional<std::vector<char>> HexStringToBinary(const CharT *lpszFormat)
{
	std::vector<char> v;
	try{
		boost::algorithm::unhex( lpszFormat, std::back_inserter(v) );
	}catch(...){	// 入力が不正だった場合
		return boost::none;
	}
	return v;
}

// バイナリデータをHEX文字列にする
template <typename InputIterator,typename OutputIterator> std::string BinaryToHexString(InputIterator b,OutputIterator e)
{
	std::string s;
	try{
		boost::algorithm::hex(b, e, std::back_inserter(s) );
	}catch(...){	// なんかしらエラー
		assert(false);
	}
	return s;
}


}

}
