#pragma once

#include <boost/beast/http.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

namespace RLib
{
namespace Http
{
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

	// URI文字列を分解
	struct CParsedUri
	{
		std::string	sScheme;		// scheme name
		std::string	sHostName;		// host name
		uint16_t	nPort = 0;		// converted port number
		std::string	sUserName;		// user name
		std::string	sPassword;		// password
		std::string	sUrlPath;		// URL-path
		std::string	sExtraInfo;		// extra information (e.g. ?foo or #foo)
	};
	CParsedUri ParseUri(const std::string &sUri);


	struct CTarget
	{
		std::string	sHost = "";
		std::string	sPort = "80";
		std::string	sTarget = "/";
		std::string	sUserName;
		std::string	sPassword;

		static CTarget ParseUri(const std::string &sUri)
		{
			const Http::CParsedUri r = Http::ParseUri(sUri);
			Http::CTarget t;
			t.sHost = r.sHostName;
			t.sPort = std::to_string(r.nPort);
			t.sTarget = r.sUrlPath + r.sExtraInfo;
			t.sUserName = r.sUserName;
			t.sPassword = r.sPassword;
			return t;
		}
	};

	struct CResult
	{
		boost::system::error_code								ec;
		http::response_parser<http::dynamic_body>::value_type	message;
	};

	typedef std::function<void (http::request<http::string_body> &req)>	FuncPreRequest;
	typedef std::function<bool (const http::request<http::string_body> &req,http::response_parser<http::dynamic_body> &res)> FuncProgress;

	// HTTP ダウンロード
	// ・リダイレクト対応
	// ・HTTPS 対応
	// ・オンメモリで処理しきれないような場合は FuncProgress でバッファから取得しつつ解放すべし
	CResult Get(		// asio::yield_context 版
		asio::io_context			&ioContext,
		const asio::yield_context	&yield,
		const CTarget				&target,
		const FuncPreRequest		&fPreRequest=nullptr,
		const FuncProgress			&fProgress=nullptr
	);

	inline CResult Get(		// 同期版
		const CTarget				&target,
		const FuncPreRequest		&fPreRequest=nullptr,
		const FuncProgress			&fProgress=nullptr
	){
		std::shared_ptr<Http::CResult> spResult;
		asio::io_context ioContext;
		asio::spawn( ioContext,
			[target,&ioContext,&spResult,&fPreRequest,&fProgress](asio::yield_context yield){
				Http::CResult r = Http::Get(ioContext,yield,target,fPreRequest,fProgress);
				spResult = std::make_shared<Http::CResult>(std::move(r));
			});
		ioContext.run();
		return std::move(*spResult);
	}


	namespace Utility
	{
		// res の bodyデータ を std::vector<char> に移動
		// ・return 移動したバイト数
		inline std::vector<char> ConsumeBody(http::response_parser<http::dynamic_body>::value_type &mes)
		{
			std::stringstream ss;
			auto& body = mes.body();
			ss << beast::buffers_to_string(body.data());
			std::vector<char> v(( std::istreambuf_iterator<char>(ss)), (std::istreambuf_iterator<char>() ));
			body.consume(v.size());
			return v;
		}

		// res の bodyデータ を std::ostream に移動
		// ・return 移動したバイト数
		inline size_t ConsumeBodyData(http::response_parser<http::dynamic_body> &res,std::ostream &os)
		{
			auto& body = res.get().body();
			const size_t size = body.size();
			if( size > 0 ){
				os << beast::buffers_to_string( body.data() );
				body.consume(size);
			}
			return size;
		}

		// res の bodyデータ を std::vector<char> に移動
		// ・return 移動したバイト数
		inline std::vector<char> ConsumeBodyDataToVector(http::response_parser<http::dynamic_body>::value_type &message)
		{
			auto& body = message.body();
			const size_t size = body.size();
			if( size <= 0 ) return std::vector<char>();

			std::vector<char> v;
			const auto nBufferSize = asio::buffer_size(body.data());
			v.reserve(nBufferSize);
			for(auto i : body.data() ){
				auto p = asio::buffer_cast<char const*>(i);
			//auto s0 = asio::buffer_size(body.data());
			//auto s1 = body.size();
			//auto s2 = i.size();
				v.insert( v.end(), p, p+i.size() );
			}
			body.consume(v.size());

if( nBufferSize != v.size() ){
	int a = 0;
}

			return v;
		}

	};



	// HTTPパーサー
	template<class Allocator, bool isRequest, class Body> void read_istream(
		std::istream& is,
		beast::basic_flat_buffer<Allocator>& buffer,
		http::message<isRequest, Body, http::fields>& msg,
		boost::system::error_code& ec
	){
		// Create the message parser
		//
		// Arguments passed to the parser's constructor are
		// forwarded to the message constructor. Here, we use
		// a move construction in case the caller has constructed
		// their message in a non-default way.
		//
		http::parser<isRequest, Body> p{ std::move(msg) };

		do{
			// Extract whatever characters are presently available in the istream
			if (is.rdbuf()->in_avail() > 0){
				// Get a mutable buffer sequence for writing
				auto const b = buffer.prepare(static_cast<std::size_t>(is.rdbuf()->in_avail()) );

				// Now get everything we can from the istream
				buffer.commit( static_cast<std::size_t>(is.readsome( reinterpret_cast<char*>(b.data()), b.size())) );
			}else if(buffer.size() == 0){
				// Our buffer is empty and we need more characters, 
				// see if we've reached the end of file on the istream
				if( !is.eof() ){
					// Get a mutable buffer sequence for writing
					auto const b = buffer.prepare(1024);

					// Try to get more from the istream. This might block.
					is.read(reinterpret_cast<char*>(b.data()), b.size());

					// If an error occurs on the istream then return it to the caller.
					if (is.fail() && !is.eof())
					{
						// We'll just re-use io_error since std::istream has no error_code interface.
						ec = make_error_code(boost::system::errc::io_error);
						return;
					}

					// Commit the characters we got to the buffer.
					buffer.commit(static_cast<std::size_t>(is.gcount()));
				}else{
					// Inform the parser that we've reached the end of the istream.
					p.put_eof(ec);
					if (ec) return;
					break;
				}
			}

			// Write the data to the parser
			auto const bytes_used = p.put(buffer.data(), ec);

			// This error means that the parser needs additional octets.
			if (ec == beast::http::error::need_more) ec = {};
			if (ec) return;

			// Consume the buffer octets that were actually parsed.
			buffer.consume(bytes_used);
		} while (!p.is_done());

		// Transfer ownership of the message container in the parser to the caller.
		msg = p.release();
	}

}
}
