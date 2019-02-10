#pragma once

#include "./String.h"
#include "./Algorithm.h"
#include "./AppLog.h"

namespace RLib
{

// ZIP�������N���X
// �E���k�A�𓀂ɂ� zlib ���g�p
// �EZIP64 �ɂ͔�Ή�
// �E�Q�lURL
//	 http://www.pkware.com/documents/casestudies/APPNOTE.TXT
//	 http://ja.wikipedia.org/wiki/ZIP_(�t�@�C���t�H�[�}�b�g)

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

	// ���[�J���ƃZ���g�����f�B���N�g���̃t�@�C���w�b�_�̋��ʕ���
	struct ZipFileHeaderCommon{
		uint16_t				versionNeededToExtract = 0;	// �W�J�ɕK�v�ȃo�[�W���� (�ŏ��o�[�W����)
		GeneralPurposeBitFlag	generalPurposeBitFlag;		// �ėp�ړI�̃r�b�g�t���O
		uint16_t				compressionMethod = 0;		// ���k���\�b�h 0:�񈳏k 8:Deflate
		uint16_t				lastModFileTime = 0;		// �t�@�C���̍ŏI�ύX����
		uint16_t				lastModFileDate = 0;		// �t�@�C���̍ŏI�ύX���t
		uint32_t				crc32 = 0;					// CRC-32
		uint32_t				compressedSize = 0;			// ���k�T�C�Y
		uint32_t				uncompressedSize = 0;		// �񈳏k�T�C�Y
		uint16_t				fileNameLength = 0;			// �t�@�C�����̒��� (n)
		uint16_t				extraFieldLength = 0;		// �g���t�B�[���h�̒��� (m)
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

	// ZIP�Z���g�����f�B���N�g���̏I�[���R�[�h(EOCD)
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

	// ZIP���[�J���t�@�C���w�b�_
	struct CLocalFileHeader{
		struct CFixed{
			uint32_t			centralFileHeaderSignature = 0;		// ���[�J���t�@�C���w�b�_�̃V�O�l�`�� = 0x504B0304�iPK\003\004�j
			ZipFileHeaderCommon	common;

			// ZIP���[�J���t�@�C���w�b�_�̃T�C�Y�擾
			uint32_t GetHeaderSize()const{
				return sizeof(CFixed) + common.fileNameLength + common.extraFieldLength;
			}

			// ���[�J���t�@�C���w�b�_�̈ʒu����ACHeader ���擾
			// �E����I���ł���� is �͎��̈ʒu�������B����ȊO�̏ꍇ�͌��̈ʒu�̂܂܁B
			static std::unique_ptr<CFixed> ReadFromStream(std::istream &is);

		}m_fixed;
		std::string			m_sFilePath;						// �t�@�C���� 
		std::vector<char>	m_vExField;							// �g���t�B�[���h

		std::string GetFilePathUtf8()const{						// �t�@�C������UTF8�Ŏ擾
			return m_fixed.common.generalPurposeBitFlag.bUTF8 ?	// UTF-8�H
				m_sFilePath : 
				String::AtoUtf8(m_sFilePath.c_str() );
		}

		// ���[�J���t�@�C���w�b�_�̈ʒu����ACLocalFileHeader ���擾
		// �E����I���ł���� is �͎��̈ʒu�������B����ȊO�̏ꍇ�͌��̈ʒu�̂܂܁B
		static std::unique_ptr<CLocalFileHeader> ReadFromStream(std::istream &is);

	};

	// �Z���g�����f�B���N�g���G���g��
	struct CCentralDirectoryEntry{
		struct CFixed{
			uint32_t			centralFileHeaderSignature = 0;	// �Z���g�����f�B���N�g���t�@�C���w�b�_�̃V�O�l�`�� = 0x504B0102�iPK\001\002�j
			uint16_t			versionMadeBy = 0;				// �쐬���ꂽ�o�[�W����
			ZipFileHeaderCommon	common;
			uint16_t			fileCommentLength = 0;			// �t�@�C���R�����g�̒��� (k)
			uint16_t			diskNumberStart = 0;				// �t�@�C�����J�n����f�B�X�N�ԍ�
			uint16_t			internalFileAttributes = 0;		// �����t�@�C������
			uint32_t			externalFileAttributes = 0;		// �O���t�@�C������
			uint32_t			relativeOffsetOfLocalHeader = 0;// ���[�J���t�@�C���w�b�_�̑��΃I�t�Z�b�g
		}m_fixed;
		std::string			m_sFilePath;						// �t�@�C����
		std::vector<char>	m_vExField;							// �g���t�B�[���h
		std::vector<char>	m_vFileComment;						// �t�@�C���R�����g

		std::string GetFilePath()const{							// �t�@�C������UTF8�Ŏ擾
			return m_fixed.common.generalPurposeBitFlag.bUTF8 ?	// UTF-8�H
				m_sFilePath : 
				String::AtoUtf8(m_sFilePath.c_str() );
		}

		// ���k�T�C�Y�擾
		std::streamoff GetCompressedSize()const{
			// TODO zip64
			return m_fixed.common.compressedSize;
		}

		// �񈳏k�T�C�Y(�𓀌�T�C�Y)�擾
		std::streamoff GetUncompressedSize()const{
			// TODO zip64
			return m_fixed.common.uncompressedSize;
		}

		// ���[�J���t�@�C���w�b�_�̈ʒu�擾
		std::streamoff GetPostionLocalHeader()const{
			// TODO zip64
			return m_fixed.relativeOffsetOfLocalHeader;
		}

	};

#pragma pack(pop)

	class CFileInfo{
		CLocalFileHeader							m_localFileHeader;
		std::shared_ptr<const std::vector<char>>	m_spCompressedData;			// ���k���ꂽ�`�̃f�[�^(ZIP�t�@�C�����̃C���[�W���̂܂܂̌`��)
		std::weak_ptr<const std::vector<char>>		m_wpUncompressedData;		// ���k����ĂȂ��`�̃f�[�^
	public:
		CFileInfo(){}
		CFileInfo(const CLocalFileHeader &localFileHeader,const std::shared_ptr<const std::vector<char>> &spCompressedData)
			:m_localFileHeader(localFileHeader)
			,m_spCompressedData(spCompressedData)
		{}
		const CLocalFileHeader& GetLocalFileHeader()const{
			return m_localFileHeader;
		}
		std::shared_ptr<const std::vector<char>> GetUncompressed()const;		// �𓀍ς݃f�[�^���擾�B�L���b�V��������΂����Ԃ��A�Ȃ���Ή𓀂��s���BCRC�`�F�b�N�i�V�B
		std::shared_ptr<const std::vector<char>> GetUncompressedWithCache();	// �𓀍ς݃f�[�^���擾�B�L���b�V��������΂����Ԃ��A�Ȃ����GetUncompressed()���ăL���b�V���ɕۑ��B
		std::shared_ptr<const std::vector<char>> GetCompressed()const{
			return m_spCompressedData;
		}
	};

	// �����f�B���N�g�����p�[�X�B�����f�B���N�g���t�@�C���w�b�_�̃��X�g���擾
	// �Ereturn first	true:����I�� false:�G���[�A���Bsecond �͓r���܂ł̃f�[�^�������Ă���
	//			second 	�����f�B���N�g�����X�g
	std::pair<bool,std::vector<CCentralDirectoryEntry>> ParseCentralDirectory(
		size_t			nEntriesCount,		// �G���g���[��
		std::istream	&is);				// �����f�B���N�g���̐擪�ʒu

	// ZIP�t�@�C����ǂݍ���
	std::pair<bool,std::vector<CFileInfo>> LoadZipFile(const char *pZipFileImage,size_t sizeZipFile);
	std::pair<bool,std::vector<CFileInfo>> LoadZipFile(std::istream &is);
#ifdef _MSC_VER
	inline std::pair<bool,std::vector<CFileInfo>> LoadZipFile(const CString &sFilePath){
		std::fstream st(sFilePath,std::ios::in|std::ios::binary,_SH_DENYWR);
		return !st.fail() ? LoadZipFile(st) : std::pair<bool,std::vector<Zip::CFileInfo>>();
	}
#endif	//#ifdef _MSC_VER

#if 0
	// ���k
	static std::unique_ptr<std::vector<char>> CompressFile(std::istream &is);
#ifdef _MSC_VER
	static std::unique_ptr<std::vector<char>> CompressFile(const CString &sFileZipPath){
		std::fstream st(sFileZipPath,std::ios::in|std::ios::binary,_SH_DENYWR);
		return !st.fail() ? CompressFile(st) : std::unique_ptr<std::vector<char>>();
	}
#endif	//#ifdef _MSC_VER
#endif

	// �𓀏���
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
			case 0:	// �����k
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
