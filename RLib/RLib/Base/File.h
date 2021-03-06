#pragma once

namespace RLib
{

namespace File
{

	struct ACL
	{
		// ACL(CSecurityDesc)を取得する
		static std::unique_ptr<CSecurityDesc> GetSecurityDesc(const std::filesystem::path &path);
		// ACL に Authenticated User を付与する
		static bool AddAuthenticatedUser(const std::filesystem::path &path);
	};

};

}

