#pragma once

#include "HttpGet.h"
#include "String.h"

namespace RLib
{
namespace Http
{

namespace GetMultiPart
{

namespace Inner
{

	struct CResult{
		boost::system::error_code									ec;
		http::response_parser<http::dynamic_body>::value_type		message;
		uint64_t													nContentSize = 0;
		std::map<std::pair<uint64_t,uint64_t>,std::vector<char>>	mapRanges;		// GetParts �ł� <<�J�n�ʒu,�I���ʒu>,�f�[�^>
																					// GetRanges �ł� <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	};

	static CResult GetParts(
		asio::io_context																	&ioContext,
		const asio::yield_context															&yield,
		const CTarget																		&target,
		const FuncPreRequest																&fPreRequest=nullptr,	// ���N�G�X�g�ʒm
		const std::function<bool (const std::pair<uint64_t,uint64_t>&,std::vector<char>&)>	&fProgress=nullptr		// �i���ʒm <<�J�n�ʒu,�I���ʒu>,�f�[�^>
	){
		CResult result;

		try{

			std::map<std::pair<uint64_t,uint64_t>,std::vector<char>> mapDownloaded;			// <<�J�n�ʒu,�I���ʒu>,�f�[�^>

			auto fDownloaded = [&mapDownloaded,&fProgress](const std::pair<uint64_t,uint64_t> &range,const std::vector<char> &vBody){
					auto &v = mapDownloaded[range];
					v.insert( v.end(), vBody.begin(), vBody.end() );
					bool b = fProgress ? fProgress( range, v ) : true;
					return b;
				};

			struct CCurrent{
				std::pair<uint64_t,uint64_t>	range;
				uint64_t						nConsumeSize = 0;
			};
			std::optional<CCurrent> current;	// ��M�����

			auto r = Http::Get( ioContext, yield, target,
				[&](http::request<http::string_body> &req){
					if( fPreRequest ) fPreRequest(req);
				},
				[&](const http::request<http::string_body> &req,http::response_parser<http::dynamic_body> &res){

					if( !res.is_header_done() ) return true;
					auto &mes = res.get();

					if( mes.result_int() == 200 ){		// Range�t������Ȃ��Ȃ畁�ʂ� HTTP GET �Ƃ��ď�������
						const int64_t nContentSize = [&mes]()->int64_t{
								auto i = mes.find(boost::beast::http::field::content_length);
								if( i != mes.end() ){
									try{
										auto n = boost::lexical_cast<int64_t>(i->value());
										return std::max<int64_t>(0,n);
									}catch(...){
									}
								}
								APPLOG_INFO("unknown content_length");
								return 0;
							}();
						auto vBody = Http::Utility::ConsumeBodyDataToVector(mes);
						return fDownloaded( {0,nContentSize}, vBody );
					}

					if( mes.result_int() == 206 ){

						// ���X�|���X�w�b�_�ɁuContent-Range: bytes 6-12/155295454�v�������݂��Ȃ���΁AMultiPart �̂���Ŏ�M����B
						// �T�[�o�[�ɂ���Ă�(akamai�Ƃ�)�ł́AContent-Type: multipart/byteranges; �����݂��Ȃ��̂ŁA����Ŕ��f���Ă̓_���B

						{// ���X�|���X�w�b�_�� Content-Range �����݂���΁A���X�|���XBODY�̓f�[�^�݂̂Ƃ��ď�������
							auto i = mes.find(boost::beast::http::field::content_range);
							if( i != mes.end() ){
								const auto name = i->name_string();
								const auto value = i->value();
								std::pair<uint64_t,uint64_t> range;
								uint64_t nContentSize;
#pragma warning( push )
#pragma warning( disable: 4996) // warning C4996: 'sscanf': This function or variable may be unsafe. Consider using sscanf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
								if( std::sscanf( value.data(), "bytes %llu-%llu/%llu",&range.first,&range.second,&nContentSize) != 3 ){
#pragma warning( pop )
									APPLOG_ERROR( "%s:%s",name,value);
									return false;		// ��M���f
								}
								result.nContentSize = nContentSize;
								auto vBody = Http::Utility::ConsumeBodyDataToVector(mes);
								return fDownloaded( range, vBody );
							}
						}

						while( true ){

							const std::string sBody = [&mes]{
									std::stringstream ss;
									ss << beast::buffers( mes.body().data() );
									return ss.str();
								}();
							if( sBody.empty() ){
								break;
							}

							if( !current ){

								// MultiPart �̃w�b�_�����擾
								std::smatch match;
								if( !std::regex_search( sBody, match, std::regex("\r?\n\r?\n") ) ){
									return true;										// �w�b�_��M���I���ĂȂ��Ȃ�return
								}
								auto nHeaderSize = match[0].second - sBody.begin();		// �w�b�_�̃T�C�Y
								mes.body().consume(nHeaderSize);						// �w�b�_��������(�폜)

								// Content-Range �擾
								if( !std::regex_search( sBody.begin(), sBody.begin()+nHeaderSize, match, std::regex("^Content-Range: bytes (\\d+)-(\\d+)/(\\d+)\\s?\r",std::regex_constants::icase)) ){
									APPLOG_ERROR("%s",sBody);
									return false;		// ��M���f
								}
								std::pair<uint64_t,uint64_t> range;
								try{
									range.first = boost::lexical_cast<uint64_t>(match[1].str());
									range.second = boost::lexical_cast<uint64_t>(match[2].str());
									uint64_t nContentSize = boost::lexical_cast<uint64_t>(match[3].str());
								}catch(...){
									APPLOG_ERROR("%s",sBody);
									return false;		// ��M���f
								}
								current = CCurrent{range,0};

								continue;
							}

							{// MultiPart �̃R���e���c���擾
								if( current->range.second < current->range.first ){
									APPLOG_ERROR("%d-%d",current->range.first,current->range.second);
									return false;		// ��M���f
								}
								const uint64_t nContentSize = current->range.second - current->range.first + 1;

								boost::string_view svBody = sBody;
								svBody = svBody.substr(0, static_cast<size_t>(nContentSize - current->nConsumeSize) );	// �R���e���c�f�[�^

								const auto nPosition = current->range.first + current->nConsumeSize;
								current->nConsumeSize += svBody.size();
								mes.body().consume( svBody.size() );								// �擾������������(�폜)

								if( !fDownloaded( {current->range.first,current->range.second}, std::vector<char>(svBody.begin(),svBody.end()) ) ){
									return false;
								}

								if( nContentSize <= current->nConsumeSize ){						// �����H
									current.reset();
								}
							}

						}
					}

					return true;
				});

			result.ec = r.ec;
			result.message = r.message;
			for( auto &i : mapDownloaded ){
				result.mapRanges[{i.first.first,i.first.second}].swap(i.second);
			}
		}catch(boost::system::error_code ec){
			APPLOG_ERROR("exception error_code");
			CResult r;
			r.ec = ec;
			return r;
		}catch(std::exception &e){
			APPLOG_ERROR("exception std::exception %s",e.what());
			throw;
		}catch(boost::coroutines::detail::forced_unwind&){
			APPLOG_ERROR("exception boost::coroutines::detail::forced_unwind");
			throw;
		}
		return result;
	}

	// range ������,��������
	// �Ereturn std::vector<std::pair< range������, �擪�̈ʒu >>
	static std::vector<std::pair<std::string,uint64_t>> RangesAnalyse(
		const std::map<uint64_t,uint64_t>	&mapRange,		// range <�J�n�ʒu,�T�C�Y> �J���̏ꍇ�A�̈悪�_�u���Ă�ꍇ�A�̈悪0�̏ꍇ�A�̓G���[�B
		uint64_t							nFillSize=1,	// �ׂ荇�����̈�(range)�̌��ԃT�C�Y���R��������������P��range�Ɍ�������B0:�������Ȃ�
		size_t								nMaxRanges=0,	// range�̐����R���ȉ��Ɏ��܂�悤�ɕ�������B0:�������Ȃ�
		size_t								nMaxBytes=0		// ���N�G�X�g�̕�����("bytes=�E�E�E")���R���ȉ��Ɏ��܂�悤�ɕ�������B0:�������Ȃ�
	){
		static const std::vector<std::pair<std::string,uint64_t>> errorResult;
		if( mapRange.empty() ){
			return errorResult;
		}
		std::map<uint64_t,uint64_t>	m = mapRange;
		for( auto i=m.begin(); true; ){
			auto j = i;
			if( ++j == m.end() ) break;
			if( i->second <= 0 ){								// �T�C�Y 0 �̓G���[
				APPLOG_ERROR( "range invalid range %d",i->second);
				return errorResult;
			}
			if( ( i->first + i->second ) > j->first ){			// �_�u���Ă�Ȃ�G���[
				APPLOG_ERROR( "range invalid range duplicated %d %d %d",i->first,i->second,j->first);
				return errorResult;
			}
			auto gap = j->first - ( i->first + i->second );		// ���Ԃ�
			if( gap < nFillSize ){								// �K��T�C�Y�ȉ���������
				i->second += gap + j->second;					// ����
				m.erase(j);
			}else{
				i++;
			}
		}
		for( auto &i : m ) i.second += i.first - 1;			// html �� rage �p�ɂ���B<�J�n�ʒu,�I���ʒu>

		const static std::string sBytes="bytes=";
		std::vector<std::pair<std::string,uint64_t>> result = { std::make_pair(sBytes,m.begin()->first) };
		size_t nRange=0;
		for( auto &i : m ){
			auto p = result.rbegin();
			const std::string s = String::Format( "%d-%d,", i.first, i.second );
			if( nRange>0 && ( ( nMaxBytes>0 && nMaxBytes<=p->first.size()+s.size()-1 ) || ( nMaxRanges>0 && nMaxRanges<=nRange ) ) ){
				p->first.pop_back();			// ������ "," �����
				nRange = 0;
				result.push_back( std::make_pair(sBytes,i.first) );
				p = result.rbegin();
			}
			p->first += s;
			nRange++;
		}
		result.rbegin()->first.pop_back();	// ������ "," �����
		return result;
	}

	// range ����������
	static std::string RangesJoin(
		const std::map<uint64_t,uint64_t>	&mapRange,		// range <�J�n�ʒu,�T�C�Y> �J���̏ꍇ�A�̈悪�_�u���Ă�ꍇ�A�̈悪0�̏ꍇ�A�̓G���[�B
		uint64_t							nFillSize=1		// �ׂ荇�����̈�(range)�̌��ԃT�C�Y���R��������������P��range�Ɍ�������B0:�������Ȃ�
	){
		auto v = RangesAnalyse(mapRange,nFillSize,0,0);
		return v.empty() ? std::string() : v[0].first;
	}

}

	struct CParameter{
		std::string					sUrl;
		std::map<uint64_t,uint64_t>	mapRange;			// range <�J�n�ʒu,�T�C�Y> �̈悪�_�u���Ă�ꍇ��̈悪0�̏ꍇ�͌��ʂ��s��
		uint16_t					nMaxRanges = 200;	// range ���N�G�X�g��(��Ԃ̐�)�ő�l�Bapache �f�t�H���g�� 200�B
	};

	struct CResult{
		struct CMessage{
			std::string												sRange;
			boost::system::error_code								ec;
			http::response_parser<http::dynamic_body>::value_type	httpMessage;
			uint64_t												nContentSize = 0;
		};
		std::vector<CMessage>										vMessage;
		std::map<std::pair<uint64_t,uint64_t>,std::vector<char>>	mapRanges;			// <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	};

	static CResult Run(
		asio::io_context			&ioContext,
		const asio::yield_context	&yield,
		const CParameter			&parameter,
		const std::function<bool (const std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*>&)> &fProgress=nullptr)	// �i���ʒm <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	{
		struct CDownload{
			uint64_t			nRangeSize = 0;			// range �T�C�Y
			uint64_t			nConsumedSize = 0;		// ���������T�C�Y
			uint64_t			nDiscardedSize = 0;		// ���ɑ����s�v����j�������T�C�Y
			std::vector<char>	vBody;					// �f�[�^
		};

		std::map<uint64_t,CDownload> mapDownload = [&]{	// <�J�n�ʒu,CDownload>
				std::remove_const<decltype(mapDownload)>::type result;
				for( auto &i : parameter.mapRange ){
					result[i.first].nRangeSize = i.second;
				}
				if( result.empty() || result.begin()->first>0 ){	// 0 �̈ʒu�͕K�v(�ԕ�)
					result[0].nRangeSize = 1;				// �T�C�Y��1
				}
				return result;
			}();

		const Http::CTarget target = Http::CTarget::ParseUri(parameter.sUrl);

		CResult result;

		const auto vRanges = Http::GetMultiPart::Inner::RangesAnalyse( parameter.mapRange, 256, parameter.nMaxRanges );
		for( auto& ranges : vRanges ){

			CResult::CMessage msg;
			std::map<uint64_t,uint64_t>	mapConsumed;
			auto r = GetMultiPart::Inner::GetParts( ioContext, yield, target, 
				[&](http::request<http::string_body> &req){
					msg.sRange = ranges.first;
					req.set( http::field::range, msg.sRange );
					APPLOG_INFO( R"(HTTP GET "%s" range:"%s")",parameter.sUrl,msg.sRange);
				},
				[&](const std::pair<uint64_t,uint64_t> &range,std::vector<char> &vBody){

					auto &nConsumed = mapConsumed[range.first];
					auto nPosition = range.first + nConsumed;
					nConsumed += vBody.size();

					std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*> mapProgress;

					while( !vBody.empty() ){
						const auto iNext = mapDownload.upper_bound(nPosition);
						auto i = iNext;
						i--;
						const auto nRemain = i->second.nRangeSize - i->second.nConsumedSize;
						if( nRemain <= 0 ){						// �s�v�f�[�^(���Ԃ̃f�[�^��)�Ȃ�
							const auto nDiscard = iNext==mapDownload.end()
								? (std::numeric_limits<uint64_t>::max)()
								: iNext->first - ( i->first + i->second.nConsumedSize + i->second.nDiscardedSize );
							const auto size = std::min<uint64_t>(nDiscard,vBody.size());
							i->second.nDiscardedSize += size;
							vBody.erase( vBody.begin(), vBody.begin()+static_cast<size_t>(size) );		// ����
							nPosition += size;										// �ʒu��i�߂�
							continue;
						}
						const auto size = std::min<uint64_t>(nRemain,vBody.size());
						auto &v = i->second.vBody;
						v.insert( v.end(), vBody.begin(), vBody.begin()+static_cast<size_t>(size) );
						i->second.nConsumedSize += size;
						vBody.erase( vBody.begin(), vBody.begin()+static_cast<size_t>(size) );		// ����
						nPosition += size;										// �ʒu��i�߂�
						if( fProgress ){
							auto f = i->first;
							if( parameter.mapRange.find(i->first) != parameter.mapRange.end() ){	// �ԕ����Ȃ�ʒm���Ȃ�
								mapProgress[{i->first,i->second.nRangeSize}] = &v;
							}
						}
					}

					if( !mapProgress.empty() ){
						if( !fProgress( mapProgress ) ){
							return false;
						}
					}

					return true;
				});

			msg.ec = r.ec;
			msg.httpMessage = r.message;
			msg.nContentSize = r.nContentSize;
			result.vMessage.push_back(msg);
			if( msg.ec ){			// �G���[������΃R�R�ŏI��
				break;
			}
		}

		if( parameter.mapRange.find(0)==parameter.mapRange.end() ) mapDownload.erase(0);		// �ԕ��͕s�v�Ȃ̂ō폜
		for( auto &i : mapDownload ){
			if( !i.second.vBody.empty() ){
				result.mapRanges[{i.first,i.second.nRangeSize}].swap(i.second.vBody);
			}
		}

		return result;

	}

}


struct CMultiPart{
	struct CResult{
		boost::system::error_code									ec;
		http::response_parser<http::dynamic_body>::value_type		message;
		uint64_t													nContentSize = 0;
		std::map<std::pair<uint64_t,uint64_t>,std::vector<char>>	mapRanges;			// <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	};

	static CResult GetRanges(
		asio::io_context											&ioContext,
		const asio::yield_context									&yield,
		const CTarget												&target,
		const std::map<uint64_t,uint64_t>							&mapRange,				// range <�J�n�ʒu,�T�C�Y> �̈悪�_�u���Ă�ꍇ��̈悪0�̏ꍇ�͌��ʂ��s��
		const FuncPreRequest										&fPreRequest=nullptr,	// ���N�G�X�g�ʒm
		const std::function<bool (const std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*>&)> &fProgress=nullptr	// �i���ʒm <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	){
		struct CDownload{
			uint64_t			nRangeSize = 0;			// range �T�C�Y
			uint64_t			nConsumedSize = 0;		// ���������T�C�Y
			uint64_t			nDiscardedSize = 0;		// ���ɑ����s�v����j�������T�C�Y
			std::vector<char>	vBody;					// �f�[�^
		};
		std::map<uint64_t,CDownload> mapDownload = [&mapRange]{	// <�J�n�ʒu,CDownload>
				std::map<uint64_t,CDownload> m;
				for( auto &i : mapRange ){
					m[i.first].nRangeSize = i.second;
				}
				if( m.empty() || m.begin()->first>0 ){	// 0 �̈ʒu�͕K�v(�ԕ�)
					m[0].nRangeSize = 1;				// �T�C�Y��1
				}
				return m;
			}();

		std::map<uint64_t,uint64_t>	mapConsumed;

		auto r = GetMultiPart::Inner::GetParts( ioContext, yield, target, fPreRequest, 
			[&](const std::pair<uint64_t,uint64_t> &range,std::vector<char> &vBody){

				auto &nConsumed = mapConsumed[range.first];
				auto nPosition = range.first + nConsumed;
				nConsumed += vBody.size();

				std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*> mapProgress;

				while( !vBody.empty() ){
					const auto iNext = mapDownload.upper_bound(nPosition);
					auto i = iNext;
					i--;
					const auto nRemain = i->second.nRangeSize - i->second.nConsumedSize;
					if( nRemain <= 0 ){						// �s�v�f�[�^(���Ԃ̃f�[�^��)�Ȃ�
						const auto nDiscard = iNext==mapDownload.end()
							? (std::numeric_limits<uint64_t>::max)()
							: iNext->first - ( i->first + i->second.nConsumedSize + i->second.nDiscardedSize );
						const auto size = std::min<uint64_t>(nDiscard,vBody.size());
						i->second.nDiscardedSize += size;
						vBody.erase( vBody.begin(), vBody.begin()+static_cast<size_t>(size) );		// ����
						nPosition += size;										// �ʒu��i�߂�
						continue;
					}
					const auto size = std::min<uint64_t>(nRemain,vBody.size());
					auto &v = i->second.vBody;
					v.insert( v.end(), vBody.begin(), vBody.begin()+static_cast<size_t>(size) );
					i->second.nConsumedSize += size;
					vBody.erase( vBody.begin(), vBody.begin()+static_cast<size_t>(size) );		// ����
					nPosition += size;										// �ʒu��i�߂�
					if( fProgress ){
						auto f = i->first;
						if( mapRange.find(i->first) != mapRange.end() ){	// �ԕ����Ȃ�ʒm���Ȃ�
							mapProgress[{i->first,i->second.nRangeSize}] = &v;
						}
					}
				}

				if( !mapProgress.empty() ){
					if( !fProgress( mapProgress ) ){
						return false;
					}
				}

				return true;
			});

		CResult result;
		result.ec = r.ec;
		result.message = r.message;
		result.nContentSize = r.nContentSize;

		if( mapRange.find(0)==mapRange.end() ) mapDownload.erase(0);		// �ԕ��͕s�v�Ȃ̂ō폜
		for( auto &i : mapDownload ){
			if( !i.second.vBody.empty() ){
				result.mapRanges[{i.first,i.second.nRangeSize}].swap(i.second.vBody);
			}
		}

		return result;
	}

	static CResult GetRanges(		// ������
		const CTarget																		&target,
		const std::map<uint64_t,uint64_t>													&mapRange,				// range <�J�n�ʒu,�T�C�Y> �̈悪�_�u���Ă�ꍇ�̓G���[
		const FuncPreRequest																&fPreRequest=nullptr,	// ���N�G�X�g�ʒm
		const std::function<bool (const std::map<std::pair<uint64_t,uint64_t>,std::vector<char>*>&)> &fProgress=nullptr	// �i���ʒm <<�J�n�ʒu,�T�C�Y>,�f�[�^>
	){
		CResult result;
		asio::io_context ioContext;
		asio::spawn( ioContext,
			[&](asio::yield_context yield){
				result = GetRanges(ioContext,yield,target,mapRange,fPreRequest,fProgress);
			});
		ioContext.run();
		return result;
	}

};

}
}
