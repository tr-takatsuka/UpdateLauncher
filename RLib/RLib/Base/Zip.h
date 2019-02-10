#pragma once

#include "./String.h"
#include "./Algorithm.h"
#include "./AppLog.h"

namespace RLib
{

// ZIPを扱うクラス
// ・圧縮、解凍には zlib を使用
// ・ZIP64 には非対応
// ・参考URL
//	 http://www.pkware.com/documents/casestudies/APPNOTE.TXT
//	 http://ja.wikipedia.org/wiki/ZIP_(ファイルフォーマット)

namespace Zip
{

#pragma pack(push, 1)
	union GeneralPurposeBitFlag{
		uint16_t all = 0;
		struct{
			uint16_t	b012	:	3;
			uint16_t	bCrc32	:	1;	// Bit 3 : crc-32, compressed size and uncompressed size are set to zero
			uint16_t	b4		:	1;	// Bit 4 : Reserved for use with method 8, for enhanced deflating
			uint16_t	b5		:	1;	// Bit 5 : compressed patched data.  (Note: Requires PKZIP version 2.70 or greater)
			uint16_t	b6		:	1;	// Bit 6 : Strong encryption.
			uint16_t	b7		:	1;	// Bit 7 : Currently unused.
			uint16_t	b8		:	1;	// Bit 8 : Currently unused.
			uint16_t	b9		:	1;	// Bit 9 : Currently unused.
			uint16_t	bA		:	1;	// Bit 10: Currently unused.
			uint16_t	bUTF8	:	1;	// Bit 11: Language encoding flag (EFS). the filename and comment fields for this file must be encoded using UTF-8.
			uint16_t	bC		:	1;	// Bit 12: Reserved by PKWARE for enhanced compression.
			uint16_t	bD		:	1;	// Bit 13: Used when encrypting the Central Directory to indicate selected data values in the Local Header are masked to hide their actual values.
			uint16_t	bE		:	1;	// Bit 14: Reserved by PKWARE.
			uint16_t	bF		:	1;	// Bit 15: Reserved by PKWARE.
		};
	};

	// ローカルとセントラルディレクトリのファイルヘッダの共通部分
	struct ZipFileHeaderCommon{
		uint16_t				versionNeededToExtract = 0;	// 展開に必要なバージョン (最小バージョン)
		GeneralPurposeBitFlag	generalPurposeBitFlag;		// 汎用目的のビットフラグ
		uint16_t				compressionMethod = 0;		// 圧縮メソッド 0:非圧縮 8:Deflate
		uint16_t				lastModFileTime = 0;		// ファイルの最終変更時間
		uint16_t				lastModFileDate = 0;		// ファイルの最終変更日付
		uint32_t				crc32 = 0;					// CRC-32
		uint32_t				compressedSize = 0;			// 圧縮サイズ
		uint32_t				uncompressedSize = 0;		// 非圧縮サイズ
		uint16_t				fileNameLength = 0;			// ファイル名の長さ (n)
		uint16_t				extraFieldLength = 0;		// 拡張フィールドの長さ (m)
	public:
		std::chrono::system_clock::time_point GetModifiedDate()const{	// Modified date
			const auto d = lastModFileDate;
			const auto t = lastModFileTime;
			std::tm tm = {0,};
			tm.tm_sec	= (t & 0x1f) * 2;
			tm.tm_min	= (t >> 5) & 0x3f;
			tm.tm_hour	= (t >> 11) & 0x1f;
			tm.tm_mday	= d & 0x1f;
			tm.tm_mon	= (d >> 5) & 0x0f - 1;
			tm.tm_year	= ( ( (d >> 9) & 0x7f) + 1980 ) - 1900;
			//tm.tm_isdst	= -1;
			return std::chrono::system_clock::from_time_t(std::mktime(&tm));
		}

	};

	// ZIPセントラルディレクトリの終端レコード(EOCD)
	struct ZipEndOfCentralDirectoryRecord{
		uint32_t	endOfCentralDirSignature = 0;
		uint16_t	numberOfThisDisk = 0;
		uint16_t	numberOfTheDiskWithTheStartOfTheCentralDirectory = 0;
		uint16_t	totalNumberOfEntries = 0;
		uint16_t	totalNumberOfEntriesInTheCentralDirectory = 0;
		uint32_t	sizeOfTheCentralDirectory = 0;
		uint32_t	offsetOfTheStartOfCentralDirectory = 0;
		uint16_t	zipFileCommentLength = 0;
	};

	// ZIPローカルファイルヘッダ
	struct CLocalFileHeader{
		struct CFixed{
			uint32_t			centralFileHeaderSignature = 0;		// ローカルファイルヘッダのシグネチャ = 0x504B0304（PK\003\004）
			ZipFileHeaderCommon	common;

			// ZIPローカルファイルヘッダのサイズ取得
			uint32_t GetHeaderSize()const{
				return sizeof(CFixed) + common.fileNameLength + common.extraFieldLength;
			}

			// ローカルファイルヘッダの位置から、CHeader を取得
			// ・正常終了であれば is は次の位置を示す。それ以外の場合は元の位置のまま。
			static std::unique_ptr<CFixed> ReadFromStream(std::istream &is);

		}m_fixed;
		std::string			m_sFilePath;						// ファイル名 
		std::vector<char>	m_vExField;							// 拡張フィールド

		std::string GetFilePathUtf8()const{						// ファイル名をUTF8で取得
			return m_fixed.common.generalPurposeBitFlag.bUTF8 ?	// UTF-8？
				m_sFilePath : 
				String::AtoUtf8(m_sFilePath.c_str() );
		}

		// ローカルファイルヘッダの位置から、CLocalFileHeader を取得
		// ・正常終了であれば is は次の位置を示す。それ以外の場合は元の位置のまま。
		static std::unique_ptr<CLocalFileHeader> ReadFromStream(std::istream &is);

	};

	// セントラルディレクトリエントリ
	struct CCentralDirectoryEntry{
		struct CFixed{
			uint32_t			centralFileHeaderSignature = 0;	// セントラルディレクトリファイルヘッダのシグネチャ = 0x504B0102（PK\001\002）
			uint16_t			versionMadeBy = 0;				// 作成されたバージョン
			ZipFileHeaderCommon	common;
			uint16_t			fileCommentLength = 0;			// ファイルコメントの長さ (k)
			uint16_t			diskNumberStart = 0;				// ファイルが開始するディスク番号
			uint16_t			internalFileAttributes = 0;		// 内部ファイル属性
			uint32_t			externalFileAttributes = 0;		// 外部ファイル属性
			uint32_t			relativeOffsetOfLocalHeader = 0;// ローカルファイルヘッダの相対オフセット
		}m_fixed;
		std::string			m_sFilePath;						// ファイル名
		std::vector<char>	m_vExField;							// 拡張フィールド
		std::vector<char>	m_vFileComment;						// ファイルコメント

		std::string GetFilePath()const{							// ファイル名をUTF8で取得
			return m_fixed.common.generalPurposeBitFlag.bUTF8 ?	// UTF-8？
				m_sFilePath : 
				String::AtoUtf8(m_sFilePath.c_str() );
		}

		// 圧縮サイズ取得
		std::streamoff GetCompressedSize()const{
			// TODO zip64
			return m_fixed.common.compressedSize;
		}

		// 非圧縮サイズ(解凍後サイズ)取得
		std::streamoff GetUncompressedSize()const{
			// TODO zip64
			return m_fixed.common.uncompressedSize;
		}

		// ローカルファイルヘッダの位置取得
		std::streamoff GetPostionLocalHeader()const{
			// TODO zip64
			return m_fixed.relativeOffsetOfLocalHeader;
		}

	};

#pragma pack(pop)

	class CFileInfo{
		CLocalFileHeader							m_localFileHeader;
		std::shared_ptr<const std::vector<char>>	m_spCompressedData;			// 圧縮された形のデータ(ZIPファイル内のイメージそのままの形式)
		std::weak_ptr<const std::vector<char>>		m_wpUncompressedData;		// 圧縮されてない形のデータ
	public:
		CFileInfo(){}
		CFileInfo(const CLocalFileHeader &localFileHeader,const std::shared_ptr<const std::vector<char>> &spCompressedData)
			:m_localFileHeader(localFileHeader)
			,m_spCompressedData(spCompressedData)
		{}
		const CLocalFileHeader& GetLocalFileHeader()const{
			return m_localFileHeader;
		}
		std::shared_ptr<const std::vector<char>> GetUncompressed()const;		// 解凍済みデータを取得。キャッシュがあればそれを返す、なければ解凍を行う。CRCチェックナシ。
		std::shared_ptr<const std::vector<char>> GetUncompressedWithCache();	// 解凍済みデータを取得。キャッシュがあればそれを返す、なければGetUncompressed()してキャッシュに保存。
		std::shared_ptr<const std::vector<char>> GetCompressed()const{
			return m_spCompressedData;
		}
	};

	// 中央ディレクトリをパース。中央ディレクトリファイルヘッダのリストを取得
	// ・return first	true:正常終了 false:エラーアリ。second は途中までのデータが入っている
	//			second 	中央ディレクトリリスト
	std::pair<bool,std::vector<CCentralDirectoryEntry>> ParseCentralDirectory(
		size_t			nEntriesCount,		// エントリー数
		std::istream	&is);				// 中央ディレクトリの先頭位置

	// ZIPファイルを読み込む
	std::pair<bool,std::vector<CFileInfo>> LoadZipFile(const char *pZipFileImage,size_t sizeZipFile);
	std::pair<bool,std::vector<CFileInfo>> LoadZipFile(std::istream &is);
#ifdef _MSC_VER
	inline std::pair<bool,std::vector<CFileInfo>> LoadZipFile(const CString &sFilePath){
		std::fstream st(sFilePath,std::ios::in|std::ios::binary,_SH_DENYWR);
		return !st.fail() ? LoadZipFile(st) : std::pair<bool,std::vector<Zip::CFileInfo>>();
	}
#endif	//#ifdef _MSC_VER

#if 0
	// 圧縮
	static std::unique_ptr<std::vector<char>> CompressFile(std::istream &is);
#ifdef _MSC_VER
	static std::unique_ptr<std::vector<char>> CompressFile(const CString &sFileZipPath){
		std::fstream st(sFileZipPath,std::ios::in|std::ios::binary,_SH_DENYWR);
		return !st.fail() ? CompressFile(st) : std::unique_ptr<std::vector<char>>();
	}
#endif	//#ifdef _MSC_VER
#endif

	// 解凍処理
	class CDecompressor{
	public:
		struct CResult
		{
			std::deque<char>	vUncomp;
			boost::crc_32_type	crc32;
			std::streamoff		nCompCount = 0;
			std::streamoff		nUncompCount = 0;
		};
	private:
		boost::iostreams::filtering_ostream	m_fo;
		CResult								m_result;
	public:
		CDecompressor(uint16_t compressionMethod)
		{
			m_fo.set_auto_close(true);
			switch( compressionMethod ){
			case 0:	// 無圧縮
				break;
			case 8:	// deflate
				{
					boost::iostreams::zlib_params p;
					p.noheader=true;
					m_fo.push( boost::iostreams::zlib_decompressor(p) );
				}
				break;
			default:
				APPLOG_ERROR("unknown compression_method:%d",compressionMethod);
				break;
			}
			m_fo.push(
				IoStream::CSink( [this](const char *p, std::streamsize size){
					m_result.vUncomp.insert(m_result.vUncomp.end(), p, p+size );
					m_result.nUncompCount += size;
					m_result.crc32.process_bytes( p, static_cast<size_t>(size) );
					return size;
				} ) );
		}

		const CResult& GetResult()const{
			return m_result;
		}

		void Push(const char *pComp,size_t size){
			m_result.nCompCount += size;
			m_fo.write(pComp,size);
			m_fo.flush();
		}

		std::deque<char> Consume(){
			std::deque<char> r;
			r.swap(m_result.vUncomp);
			return r;
		}
	};

};

}
