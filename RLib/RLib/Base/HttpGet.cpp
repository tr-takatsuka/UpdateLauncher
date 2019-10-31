#include "RLib/RLibPrecompile.h"
#include ".\HttpGet.h"

#include <boost/beast/core.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

#pragma comment(lib,"crypt32")
#pragma comment(lib,"libcrypto.lib")
#pragma comment(lib,"libssl.lib")
#pragma comment(lib,"wininet.lib")

#ifdef _MSC_VER
#include <afxinet.h>
#endif	//#ifdef _MSC_VER


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

using namespace boost;
using namespace RLib;
using namespace RLib::Http;


Http::CParsedUri Http::ParseUri(const std::string &sUri)
{
	CParsedUri r;

#ifdef _MSC_VER

	auto f = [](const std::string &sUri){
		CParsedUri r;
		const CString sSrc( CA2T(sUri.c_str(),CP_UTF8) );		// UTF8 → CString

		const size_t size = sSrc.GetLength();

		std::vector<TCHAR> vScheme(size+1);
		std::vector<TCHAR> vHostName(size+1);
		std::vector<TCHAR> vUserName(size+1);
		std::vector<TCHAR> vPassword(size+1);
		std::vector<TCHAR> vUrlPath(size+1);
		std::vector<TCHAR> vExtraInfo(size+1);

		URL_COMPONENTS uc;
		uc.dwStructSize = sizeof(uc);
		uc.lpszScheme		= &vScheme[0];
		uc.dwSchemeLength	= vScheme.size();
		uc.lpszHostName		= &vHostName[0];
		uc.dwHostNameLength	= vHostName.size();
		uc.lpszUserName		= &vUserName[0];
		uc.dwUserNameLength	= vUserName.size();
		uc.lpszPassword		= &vPassword[0];
		uc.dwPasswordLength	= vPassword.size();
		uc.lpszUrlPath		= &vUrlPath[0];
		uc.dwUrlPathLength	= vUrlPath.size();
		uc.lpszExtraInfo	= &vExtraInfo[0];
		uc.dwExtraInfoLength= vUrlPath.size();

		if( ::InternetCrackUrl(sSrc, 0, 0, &uc) ){
			r.sScheme	= CT2A(uc.lpszScheme,CP_UTF8);
	//		r.nScheme	= uc.nScheme;
			r.sHostName	= CT2A(uc.lpszHostName,CP_UTF8);
			r.nPort		= uc.nPort;
			r.sUserName	= CT2A(uc.lpszUserName,CP_UTF8);
			r.sPassword	= CT2A(uc.lpszPassword,CP_UTF8);
			r.sUrlPath	= CT2A(uc.lpszUrlPath,CP_UTF8);
			r.sExtraInfo= CT2A(uc.lpszExtraInfo,CP_UTF8);
		}
		return r;
	};
	r = f(sUri);
	if( r.nPort == 0 ) r = f( "http://" + sUri );		// スキーム(HTTP)をつけて解釈を試みる

#endif	//#ifdef _MSC_VER

	return r;
}


Http::CResult Http::Get(asio::io_context &ioContext,const asio::yield_context &yield,const CTarget &target,const FuncPreRequest &fPreRequest,const FuncProgress &fProgress)
{
	auto func = [&ioContext,&yield,&fPreRequest,&fProgress](const CTarget &target){

		boost::system::error_code	ec;
		http::response_parser<http::dynamic_body> res;

		res.body_limit( (std::numeric_limits<uint64_t>::max)() );

		int version = 10;// : 11;

		// The SSL context is required, and holds certificates
		//ssl::context ctx{ssl::context::sslv23_client};
		auto ctx = asio::ssl::context{asio::ssl::context::sslv23};

		// These objects perform our I/O
		asio::ip::tcp::resolver resolver{ioContext};
		asio::ssl::stream<asio::ip::tcp::socket> stream{ioContext, ctx};

		// Set SNI Hostname (many hosts need this to handshake successfully)
		if(! SSL_set_tlsext_host_name(stream.native_handle(), target.sHost.c_str())){
			ec.assign(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category());
			//std::cerr << ec.message() << "\n";
			return CResult{ec,res.release()};
		}

		// Look up the domain name
		auto const results = resolver.async_resolve( target.sHost, target.sPort, yield[ec]);
		if( ec ) return CResult{ec,res.release()};

		// Make the connection on the IP address we get from a lookup
		asio::async_connect(stream.next_layer(), results.begin(), results.end(), yield[ec]);
		if( ec ) return CResult{ec,res.release()};

		// Perform the SSL handshake
		stream.async_handshake(asio::ssl::stream_base::client, yield[ec]);
		if( ec ) return CResult{ec,res.release()};

		// Set up an HTTP GET request message
		http::request<http::string_body> req{http::verb::get, target.sTarget, version};
		req.set( http::field::host, target.sHost );
	//	req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		// basic 認証
		if( !target.sUserName.empty() || !target.sPassword.empty() ){

			const auto base64Encode = [](const std::string &s){
				std::string dst;
				dst.resize(beast::detail::base64::encoded_size(s.size()));
				dst.resize(beast::detail::base64::encode(&dst.at(0), s.data(), s.size()));
				return dst;
			};
			const std::string sAuth = base64Encode( target.sUserName + ":" + target.sPassword );

			req.set(http::field::authorization, "Basic "+sAuth );
		}

		// リクエストハンドラ呼び出し
		if( fPreRequest ) fPreRequest(req);

/*
		{
			http::request_serializer<http::string_body> sr{req};
			std::stringstream ss;
			ss << sr.get();
			auto s = ss.str();
			RTRACE("\n%s",s);
		}
*/

		// Send the HTTP request to the remote host
		//http::write(stream, req);
		http::async_write(stream, req, yield[ec]);
		if( ec ) return CResult{ec,res.release()};

		// This buffer is used for reading and must be persisted

		// Declare a container to hold the response
	//	http::response<http::dynamic_body> res;
	//	http::response_parser<http::dynamic_body> res;
	//	http::response_parser<http::string_body> res;
	//	http::parser<false,http::empty_body> res;

		// Receive the HTTP response
		beast::flat_buffer buf;
		buf.prepare(1024*32);						// バッファサイズ
		while(true){
			http::async_read_some(stream, buf, res, yield[ec]);
			if( ec ) return CResult{ec,res.release()};

			if( fProgress ){						// 通知
				try{
					if( !fProgress(req,res) ){		// 中断なら抜ける
						break;
					}
				}catch(boost::coroutines::detail::forced_unwind&){
					throw;
				}catch(...){
					stream.async_shutdown( yield[ec] );
					throw;
				}
			}

			if( res.is_done() ){				// 完了なら抜ける
				break;
			}

		}

		// Gracefully close the stream
		stream.async_shutdown( yield[ec] );
		if( ec == asio::error::eof ){
			// Rationale:
			// http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
			ec.assign( 0, ec.category() );
		}
		if( ec ) return CResult{ec,res.release()};

		// If we get here then the connection is closed gracefully
		return CResult{ec,res.release()};

	};

	CTarget t = target;
	for(int n=0; n<5; n++ ){		// リダイレクト許容回数
		auto result = func(t);

		if( result.ec ){
			return result;
		}

		{// リダイレクト対応
			auto &message = result.message;
			auto nHttpStatus = message.result_int();
			if( nHttpStatus==301 || nHttpStatus==302 ){
				auto i = message.find(http::field::location);
				if( i != message.end() ){
					CParsedUri parsed = ParseUri( i->value().to_string() );
					t.sHost = parsed.sHostName;
					t.sPort = std::to_string(parsed.nPort);
					t.sTarget = parsed.sUrlPath + parsed.sExtraInfo;
					continue;
				}
			}
		}

		return CResult{result.ec,std::move(result.message)};
	}
	return CResult();
}

