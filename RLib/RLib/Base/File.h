#pragma once

namespace RLib
{

namespace File
{

	struct ACL
	{
		// ACL(CSecurityDesc)‚ðŽæ“¾‚·‚é
		static std::unique_ptr<CSecurityDesc> GetSecurityDesc(const std::filesystem::path &path);
		// ACL ‚É Authenticated User ‚ð•t—^‚·‚é
		static bool AddAuthenticatedUser(const std::filesystem::path &path);
	};

};

}

