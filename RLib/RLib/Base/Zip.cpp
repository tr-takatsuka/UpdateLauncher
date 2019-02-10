// RNumberRect.cpp: CRNumberRect �N���X�̃C���v�������e�[�V����
//
//////////////////////////////////////////////////////////////////////

#include "RLib/RLibPrecompile.h"
#include "Zip.h"

#include "RLib/Base/AppLog.h"
#include "RLib/Base/Algorithm.h"

//#include "RLib/external/zlib/zlib.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

using namespace RLib;

// PK0304
#define ZipLocalFileHeader_SIGNATURE 0x04034B50
// PK0102
#define ZipFileHeader_SIGNATURE 0x02014B50
// PK0506
#define ZipEndOfCentralDirectoryRecord_SIGNATURE 0x06054B50

std::unique_ptr<Zip::CLocalFileHeader::CFixed> Zip::CLocalFileHeader::CFixed::ReadFromStream(std::istream &is){
	auto upFixed = std::make_unique<CLocalFileHeader::CFixed>();
	const auto pos = is.tellg();
	if( is.read(reinterpret_cast<char*>(upFixed.get()),sizeof(*upFixed)).gcount() == sizeof(*upFixed) ){
		if( ::memcmp(&upFixed->centralFileHeaderSignature,"PK""\3""\4",sizeof(upFixed->centralFileHeaderSignature)) == 0 ){	// �V�O�l�`���`�F�b�N "PK" "\3" "\4"
			return upFixed;
		}
		APPLOG_ERROR("error signature.");
	}
	is.seekg(pos);
	return nullptr;
}

std::unique_ptr<Zip::CLocalFileHeader> Zip::CLocalFileHeader::ReadFromStream(std::istream &is){
	const auto pos = is.tellg();
	auto upLocalFileHeader  = std::make_unique<CLocalFileHeader>();
	if( auto upFixed = CLocalFileHeader::CFixed::ReadFromStream(is) ){
		upLocalFileHeader->m_fixed = *upFixed;
		// Filename
		auto &sPath = upLocalFileHeader->m_sFilePath;
		sPath.resize(upLocalFileHeader->m_fixed.common.fileNameLength);
		if( sPath.length()<=0 || is.read( &sPath.at(0),sPath.length() ).gcount() == static_cast<std::streamsize>(sPath.length()) ){
			// extra field
			auto &sExField = upLocalFileHeader->m_vExField;
			sExField.resize( upLocalFileHeader->m_fixed.common.extraFieldLength );
			if( sExField.size()<=0 || is.read( &upLocalFileHeader->m_vExField[0], upLocalFileHeader->m_vExField.size() ).gcount() == static_cast<std::streamsize>(upLocalFileHeader->m_vExField.size()) ){
				return upLocalFileHeader;
			}
		}
	}
	is.seekg(pos);
	return nullptr;
}


// �����f�B���N�g�����p�[�X
std::pair<bool,std::vector<Zip::CCentralDirectoryEntry>> Zip::ParseCentralDirectory(size_t nEntriesCount,std::istream &is){
	std::pair<bool,std::vector<CCentralDirectoryEntry>> result;
	result.first = true;

	try{
		for( size_t i=0; i<nEntriesCount; i++ ){

			CCentralDirectoryEntry cdfh;

			if( is.read(reinterpret_cast<char*>(&cdfh.m_fixed),sizeof(cdfh.m_fixed)).gcount() < sizeof(cdfh.m_fixed) ){
				result.first = false;
				break;
			}

			// �V�O�l�`���`�F�b�N�@"PK" "\1" "\2"
			if( ::memcmp(&cdfh.m_fixed.centralFileHeaderSignature,"PK""\1""\2",sizeof(cdfh.m_fixed.centralFileHeaderSignature)) != 0 ){
				APPLOG_ERROR("error signature.");
				result.first = false;
				break;
			}

			// Filename
			cdfh.m_sFilePath.resize(cdfh.m_fixed.common.fileNameLength);
			if( cdfh.m_sFilePath.length() > 0 ){
				if( is.read( reinterpret_cast<char*>(&cdfh.m_sFilePath.at(0)), cdfh.m_sFilePath.length() ).gcount() < static_cast<int>(cdfh.m_sFilePath.length()) ){
					result.first = false;
					break;
				}
			}

			// extra field
			cdfh.m_vExField.resize( cdfh.m_fixed.common.extraFieldLength );
			if( cdfh.m_vExField.size() > 0 ){
				if( static_cast<size_t>(is.read( &cdfh.m_vExField[0], cdfh.m_vExField.size() ).gcount()) < cdfh.m_vExField.size() ){
					result.first = false;
					break;
				}
			}
			
			// file comment
			cdfh.m_vFileComment.resize( cdfh.m_fixed.fileCommentLength );
			if( cdfh.m_vFileComment.size() > 0 ){
				if( static_cast<size_t>(is.read( &cdfh.m_vFileComment[0], cdfh.m_vFileComment.size() ).gcount()) < cdfh.m_vFileComment.size() ){
					result.first = false;
					break;
				}
			}
			
			result.second.push_back(cdfh);
		}
	}catch(...){
		result.first = false;
	}
	return result;
}

// ZIP�t�@�C����ǂݍ���
std::pair<bool,std::vector<Zip::CFileInfo>> Zip::LoadZipFile(const char *pZipFileImage,size_t sizeZipFile)
{
	std::pair<bool,std::vector<Zip::CFileInfo>> result;
	result.first = true;

	try{

		// �����f�B���N�g���t�@�C���w�b�_���X�g��ǂ݂���
		std::vector<CCentralDirectoryEntry> vCentralDirectoryFileHeader;
		if( sizeZipFile <= sizeof(ZipEndOfCentralDirectoryRecord) ){		// �T�C�Y�`�F�b�N
			result.first = false;
		}else{
			const ZipEndOfCentralDirectoryRecord &endRecord = *reinterpret_cast<const ZipEndOfCentralDirectoryRecord*>(&pZipFileImage[sizeZipFile-sizeof(ZipEndOfCentralDirectoryRecord)]);
			unsigned int n = endRecord.offsetOfTheStartOfCentralDirectory;
			if( sizeZipFile > n ){										// �T�C�Y�`�F�b�N
				std::istringstream is( std::string(&pZipFileImage[n], sizeZipFile - n) );
				std::pair<bool,std::vector<CCentralDirectoryEntry>> r = ParseCentralDirectory(endRecord.totalNumberOfEntries,is);	// �����f�B���N�g���t�@�C���w�b�_���X�g���擾
				result.first = r.first;
				vCentralDirectoryFileHeader = r.second;
			}
		}

		// CompressedData
		for(std::vector<CCentralDirectoryEntry>::const_iterator i=vCentralDirectoryFileHeader.begin(); i!=vCentralDirectoryFileHeader.end(); i++ ){
			const CCentralDirectoryEntry &cdfh = *i;
			const CCentralDirectoryEntry::CFixed &header = cdfh.m_fixed;
			if( sizeZipFile <= header.relativeOffsetOfLocalHeader + sizeof(CLocalFileHeader::CFixed) ){	// �T�C�Y�`�F�b�N
				result.first = false;					// �G���[����
			}else{
				std::istringstream is( std::string(&pZipFileImage[header.relativeOffsetOfLocalHeader], sizeZipFile - header.relativeOffsetOfLocalHeader ));

				auto localFileHeader = Zip::CLocalFileHeader::ReadFromStream(is);
//TRACE("\n%s",r.second.GetFilePathUtf8());
				if( !localFileHeader ){
					result.first = false;				// �G���[����
				}else{
					std::shared_ptr<std::vector<char>> spBody(new std::vector<char>(localFileHeader->m_fixed.common.compressedSize));
					if( spBody->size()==0 || static_cast<size_t>(is.read(&(*spBody)[0],spBody->size()).gcount())>=spBody->size() ){
						result.second.push_back(CFileInfo(*localFileHeader,spBody));
					}else{
						result.first = false;			// �G���[����
					}
				}
//TRACE(_T(" OK"));
			}
		}

	}catch(...){
		result.first = false;
	}

	return result;
}

std::pair<bool,std::vector<Zip::CFileInfo>> Zip::LoadZipFile(std::istream &is)
{
	const std::istreambuf_iterator<char> begin(is);

	std::vector<char> vFileImage;
	{
//		static CRTimePerformance _tp(_T("%s(%d)"),CString(__FUNCTION__),__LINE__);
//		CRTimePerformance::CCountUp _tpc(_tp);
//		std::swap( vFileImage, std::vector<char>(begin,std::istreambuf_iterator<char>()) );	// 11661ms
//		std::copy(begin,std::istreambuf_iterator<char>(),back_inserter(vFileImage));	// 7018ms
		vFileImage.resize(static_cast<size_t>(is.seekg(0,std::ios::end).tellg()));							// 959ms
		is.seekg(0,std::ios::beg).read(&vFileImage[0], vFileImage.size());
	}
	if( vFileImage.empty() ) return std::pair<bool,std::vector<Zip::CFileInfo>>();	// �G���[�H
	return LoadZipFile(&vFileImage[0],vFileImage.size());
}

#if 0
// ���k zlib
std::unique_ptr<std::vector<char>> Deflate(std::istream &is)
{
	std::unique_ptr<std::vector<char>> apvResult(new std::vector<char>);

	z_stream z = {0,};
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;
//	if( ::deflateInit(&z, Z_DEFAULT_COMPRESSION) != Z_OK ){
	if( ::deflateInit2( &z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY ) ){
		BOOST_ASSERT(false);
		return std::unique_ptr<std::vector<char>>();
	}

	struct CEnd{	// ��n��
		z_stream *const	p;
		CEnd(z_stream &z)
			:p(&z)
			{}
		~CEnd()
			{
				BOOST_VERIFY( ::deflateEnd(p) == Z_OK );
			}
	}end(z);

	std::vector<Byte> vIn(4096),vOut(4096);		// ���́A�o�̓o�b�t�@
	z.next_out = &vOut[0];						// �o�̓|�C���^
	z.avail_out = vOut.size();					// �o�̓o�b�t�@�c��
	int nBufErrorContinue = 0;					// Z_BUF_ERROR ���J��Ԃ����Ƃ��̓G���[�Ƃ���
	for(int status=Z_OK; status!=Z_STREAM_END; ){
		int flush = Z_NO_FLUSH;
		if( z.avail_in == 0 ){					// ���͎c�ʂ��[���ɂȂ��
			z.next_in = &vIn[0];				// ���̓|�C���^�����ɖ߂�
			z.avail_in = static_cast<uInt>(is.read( reinterpret_cast<char*>(&vIn[0]), vIn.size() ).gcount());
			is.clear();							// �N���A���Ă���
			if( z.avail_in < vIn.size() ) flush = Z_FINISH;	// ���͂��Ō�ɂȂ����� deflate() �̑�2������ Z_FINISH �ɂ���
		}
		status = ::deflate(&z,flush);			// ���k
		if( status == Z_BUF_ERROR ){
			if( ++nBufErrorContinue > 4 ){		// �J��Ԃ����Ƃ��̓G���[
				BOOST_ASSERT(false);
				return std::unique_ptr<std::vector<char>>();
			}
		}else{
			nBufErrorContinue = 0;
		}
		switch( status ){
		case Z_STREAM_END:
		case Z_OK:
		case Z_BUF_ERROR:						// ���ꂪ���邱�Ƃ�����
			{
				const int nOutSize = vOut.size() - z.avail_out;
				if( nOutSize > 0) {
					apvResult->insert( apvResult->end(), vOut.begin(), vOut.begin()+nOutSize );
				}
				z.next_out = &vOut[0];			// �o�̓|�C���^
				z.avail_out = vOut.size();		// �o�̓o�b�t�@�c��
			}
			break;
		default:	// ����ȊO��������G���[
			return std::unique_ptr<std::vector<char>>();
		}
	}

	return apvResult;
}

// �� zlib
std::unique_ptr<std::vector<char>> Inflate(std::istream &is)
	std::unique_ptr<std::vector<char>> apvResult(new std::vector<char>);

    z_stream z = {0,};
    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;
	if( ::inflateInit2(&z,-MAX_WBITS) != Z_OK ){
		BOOST_ASSERT(false);
		return std::unique_ptr<std::vector<char>>();
	}

	struct CEnd	// ��n��
	{
		z_stream *const	p;
		CEnd(z_stream &z)
			:p(&z)
			{}
		~CEnd()
			{
				BOOST_VERIFY( ::inflateEnd(p) == Z_OK );
			}
	}end(z);

	std::vector<Byte> vIn(4096),vOut(4096);		// ���́A�o�̓o�b�t�@
	z.next_out = &vOut[0];						// �o�̓|�C���^
	z.avail_out = vOut.size();					// �o�̓o�b�t�@�c��
	int nBufErrorContinue = 0;					// Z_BUF_ERROR ���J��Ԃ����Ƃ��̓G���[�Ƃ���
	for(int status=Z_OK; status!=Z_STREAM_END; ){
		int flush = Z_NO_FLUSH;
		if( z.avail_in == 0 ){					// ���͎c�ʂ��[���ɂȂ��
			z.next_in = &vIn[0];				// ���̓|�C���^�����ɖ߂�
			z.avail_in = static_cast<uInt>(is.read( reinterpret_cast<char*>(&vIn[0]), vIn.size() ).gcount());
			is.clear();							// �N���A���Ă���
			if( z.avail_in < vIn.size() ) flush = Z_FINISH;	// ���͂��Ō�ɂȂ����� deflate() �̑�2������ Z_FINISH �ɂ���
		}
		status = ::inflate(&z,flush);			// �W�J
		if( status == Z_BUF_ERROR ){
			if( ++nBufErrorContinue > 4 ){		// �J��Ԃ����Ƃ��̓G���[
				BOOST_ASSERT(false);
				return std::unique_ptr<std::vector<char>>();
			}
		}else{
			nBufErrorContinue = 0;
		}
		switch( status ){
		case Z_BUF_ERROR:						// ���ꂪ���邱�Ƃ�����
		case Z_STREAM_END:
		case Z_OK:
			{
				const int nOutSize = vOut.size() - z.avail_out;
				if( nOutSize > 0) {
					apvResult->insert( apvResult->end(), vOut.begin(), vOut.begin()+nOutSize );
				}
				z.next_out = &vOut[0];			// �o�̓|�C���^
				z.avail_out = vOut.size();		// �o�̓o�b�t�@�c��
			}
			break;
		default:	// ����ȊO��������G���[
			return std::unique_ptr<std::vector<char>>();
		}
	}

	return apvResult;
}

std::unique_ptr<std::vector<char>> Zip::CompressFile(std::istream &is)
{
#if 1
	std::unique_ptr<std::vector<char>> apCompress = ::Deflate(is);									// ���k
#else					// �m�F����
	const std::istream::pos_type current = is.tellg();			// �ǂݍ��݈ʒu�m��
	const uint32_t crc32src = CGetCRC32()( is );			// ���k�O��CRC
	is.seekg(current);											// �߂�
	std::unique_ptr<std::vector<char>> apCompress = ::Deflate(is);		// ���k
	std::unique_ptr<std::vector<char>> apUncompress = ::Inflate(std::istringstream(std::string(&(*apCompress)[0],apCompress->size())));	// ���k�������̂�W�J
	if( apUncompress.get() ){
		const uint32_t crc32dst = CGetCRC32()(&(*apUncompress)[0],apUncompress->size());
		BOOST_ASSERT( crc32src == crc32dst );
	}else{
		BOOST_ASSERT(false);
	}
#endif
	return apCompress;
}

#endif

std::shared_ptr<const std::vector<char>> Zip::CFileInfo::GetUncompressed()const{
	std::shared_ptr<const std::vector<char>> spUncompressed = m_wpUncompressedData.lock();
	if( spUncompressed ) return spUncompressed;

	if( !m_spCompressedData ) return std::shared_ptr<const std::vector<char>>();
	if( m_spCompressedData->size()<=0 ) return std::shared_ptr<const std::vector<char>>(new std::vector<char>);

	if( m_localFileHeader.m_fixed.common.compressionMethod == 0 ){		// �񈳏k
		spUncompressed = m_spCompressedData;
	}else{
///		spUncompressed = Inflate( std::istringstream(std::string(&(*m_spCompressedData)[0],m_spCompressedData->size())) );

/*
		CFileInfo &fi = *this;
		bool bCheckOk = fi.
		{// CRC�`�F�b�N
			if( spUncompressed && spUncompressed->size()>0 ){
				const uint32_t crc32 = CGetCRC32()( &(*spUncompressed)[0], spUncompressed->size() );
				if( crc32 != m_localFileHeader.m_header.common.crc32 ){		// CRC�G���[
					BOOST_ASSERT(false);
				}
			}
		}
*/

	}

	return spUncompressed;
}

std::shared_ptr<const std::vector<char>> Zip::CFileInfo::GetUncompressedWithCache(){
	m_wpUncompressedData = GetUncompressed();
	return m_wpUncompressedData.lock();
}

