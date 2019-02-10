// stdafx.cpp : 標準インクルード RLib.pch のみを
// 含むソース ファイルは、プリコンパイル済みヘッダーになります。
// stdafx.obj にはプリコンパイル済み型情報が含まれます。

#include "RLib/RLibPrecompile.h"

// TODO: このファイルではなく、STDAFX.H で必要な
// 追加ヘッダーを参照してください。

namespace boost
{

void assertion_failed(char const * expr, char const * function, char const * file, long line) // user defined
{
	CStringA s;
	s.Format("\nassertion_failed \"%s\"\n\t%s\n\t%s(%d)\n",expr,function,file,line);
	ATLTRACE( s );
//	throw std::runtime_error( std::string(s) );
}

void assertion_failed_msg(const char* expr, const char* msg, const char* function,const char* file, long line)
{
	CStringA s;
	s.Format("\nassertion_failed \"%s\"\n\t\"%s\"\n\t%s\n\t%s(%d)\n",expr,msg,function,file,line);
	ATLTRACE( s );
	//throw std::runtime_error( std::string(s) );
}

} // namespace boost
