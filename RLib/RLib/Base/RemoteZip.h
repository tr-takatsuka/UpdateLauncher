#pragma once

#include "./Zip.h"

namespace RLib
{

namespace RemoteZip
{
namespace asio = boost::asio;

	// ZIP ���[�J���t�@�C���w�b�_�̈ʒu�ɂ���r�֐�
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

	// ZIP �Z���g�����f�B���N�g�����擾
	struct CCentralDirectories{
		std::set<std::shared_ptr<const Zip::CCentralDirectoryEntry>,CLessEntry>	entries;				// Central directory file headeries
		std::shared_ptr<const Zip::ZipEndOfCentralDirectoryRecord>				spEndOfCentralDirectory;// End of central directory record (EOCD)
	};
	CCentralDirectories GetCentralDirectories(asio::io_context &ioContext,const asio::yield_context &yield,const std::string &sUrl);


	// ZIP �̃t�@�C���_�E�����[�h
	// �E�K�v�ȉӏ������_�E�����[�h����
	namespace DownloadFileData
	{
		struct CEntryInfo{
			std::streamoff						nDownloadedSize = 0;	// �_�E�����[�h�σT�C�Y
			std::shared_ptr<std::deque<char>>	spData;					// �_�E�����[�h�σf�[�^
		};

		using CMapResult = std::map<std::shared_ptr<const Zip::CCentralDirectoryEntry>,CEntryInfo,CLessEntry>;

		struct CParameter{
			std::string																		sUrl;					// zip �t�@�C���� url
			CCentralDirectories																centralDirectories;		//
			std::map<std::shared_ptr<const Zip::CCentralDirectoryEntry>,std::streamoff,CLessEntry>	mapTarget;		// �_�E�����[�h�Ώ�Entry <Entry,�_�E�����[�h�σT�C�Y>
			uint16_t																		nMaxRanges = 200;		// range ���N�G�X�g��(��Ԃ̐�)�ő�l���w��
		};

		CMapResult Run(
			asio::io_context			&ioContext,
			const asio::yield_context	&yield,
			const CParameter			&parameter,
			const std::function<bool (const CMapResult&)>	&fProgress=nullptr	// �i���ʒm
		);
	}

}

}
