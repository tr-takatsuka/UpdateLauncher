#pragma once

#include "./Zip.h"

namespace RLib
{

namespace RemoteZip
{
namespace asio = boost::asio;

	// ZIP ローカルファイルヘッダの位置による比較関数
	struct CLessEntry{
		typedef void is_transparent;
		bool operator()(const std::shared_ptr<const Zip::CCentralDirectoryEntry> &a, const std::shared_ptr<const Zip::CCentralDirectoryEntry> &b)const{
			return a->GetPostionLocalHeader() < b->GetPostionLocalHeader();
		}
		bool operator()(std::streamoff a, const std::shared_ptr<const Zip::CCentralDirectoryEntry> &b)const{
			return a < b->GetPostionLocalHeader();
		}
		bool operator()(const std::shared_ptr<const Zip::CCentralDirectoryEntry> &a, std::streamoff b)const{
			return a->GetPostionLocalHeader() < b;
		}
	};

	// ZIP セントラルディレクトリ情報取得
	struct CCentralDirectories{
		std::set<std::shared_ptr<const Zip::CCentralDirectoryEntry>,CLessEntry>	entries;				// Central directory file headeries
		std::shared_ptr<const Zip::ZipEndOfCentralDirectoryRecord>				spEndOfCentralDirectory;// End of central directory record (EOCD)
	};
	CCentralDirectories GetCentralDirectories(asio::io_context &ioContext,const asio::yield_context &yield,const std::string &sUrl);


	// ZIP のファイルダウンロード
	// ・必要な箇所だけダウンロードする
	namespace DownloadFileData
	{
		struct CEntryInfo{
			std::streamoff						nDownloadedSize = 0;	// ダウンロード済サイズ
			std::shared_ptr<std::deque<char>>	spData;					// ダウンロード済データ
		};

		using CMapResult = std::map<std::shared_ptr<const Zip::CCentralDirectoryEntry>,CEntryInfo,CLessEntry>;

		struct CParameter{
			std::string																		sUrl;					// zip ファイルの url
			CCentralDirectories																centralDirectories;		//
			std::map<std::shared_ptr<const Zip::CCentralDirectoryEntry>,std::streamoff,CLessEntry>	mapTarget;		// ダウンロード対象Entry <Entry,ダウンロード済サイズ>
			uint16_t																		nMaxRanges = 200;		// range リクエストの(区間の数)最大値を指定
		};

		CMapResult Run(
			asio::io_context			&ioContext,
			const asio::yield_context	&yield,
			const CParameter			&parameter,
			const std::function<bool (const CMapResult&)>	&fProgress=nullptr	// 進捗通知
		);
	}

}

}
