
#include "RLib/RLibPrecompile.h"

#include "File.h"
//#include  <io.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace boost;
using namespace RLib;


// ACL(CSecurityDesc)���擾����
std::unique_ptr<CSecurityDesc> File::ACL::GetSecurityDesc(const std::filesystem::path &path)
{
	DWORD dwSDSize = 0;
	::GetFileSecurity(path.generic_wstring().c_str(), DACL_SECURITY_INFORMATION, NULL, 0, &dwSDSize);			// �K�v�T�C�Y�擾
	if( dwSDSize <= 0 ){
		BOOST_ASSERT(false);
		return std::unique_ptr<CSecurityDesc>();
	}
	std::vector<char> vBuf(dwSDSize);
	if( !::GetFileSecurity(path.generic_wstring().c_str(), DACL_SECURITY_INFORMATION, &vBuf[0], dwSDSize, &dwSDSize) ){
		BOOST_ASSERT(false);
		return std::unique_ptr<CSecurityDesc>();
	}
	std::unique_ptr<CSecurityDesc> ap(new CSecurityDesc(*reinterpret_cast<SECURITY_DESCRIPTOR*>(&vBuf[0])));
	return ap;
}

// ACL �� Authenticated User ��t�^����
bool File::ACL::AddAuthenticatedUser(const std::filesystem::path &path)
{
	const std::unique_ptr<CSecurityDesc> apSecurityDesc = GetSecurityDesc(path);
	if( !apSecurityDesc.get() ){
		BOOST_ASSERT(false);
		return false;
	}
	CDacl dacl;
	apSecurityDesc->GetDacl(&dacl);
	dacl.AddAllowedAce(Sids::AuthenticatedUser(), GENERIC_ALL, CONTAINER_INHERIT_ACE|OBJECT_INHERIT_ACE);	// Authenticated User ��ǉ�
	apSecurityDesc->SetDacl(dacl);

	SECURITY_DESCRIPTOR sd = *apSecurityDesc->GetPSECURITY_DESCRIPTOR();
	BOOL bRet = ::SetFileSecurity(path.generic_wstring().c_str(), DACL_SECURITY_INFORMATION, &sd );			// �Z�L�����e�B������ݒ�
	if( !bRet ){
		HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
		BOOST_ASSERT( false );
		return false;
	}
	return true;
}
