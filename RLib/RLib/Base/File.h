#pragma once

namespace RLib
{

namespace File
{

	struct ACL
	{
		// ACL(CSecurityDesc)���擾����
		static std::unique_ptr<CSecurityDesc> GetSecurityDesc(const std::filesystem::path &path);
		// ACL �� Authenticated User ��t�^����
		static bool AddAuthenticatedUser(const std::filesystem::path &path);
	};

};

}

