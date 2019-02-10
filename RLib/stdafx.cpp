// stdafx.cpp : �W���C���N���[�h RLib.pch �݂̂�
// �܂ރ\�[�X �t�@�C���́A�v���R���p�C���ς݃w�b�_�[�ɂȂ�܂��B
// stdafx.obj �ɂ̓v���R���p�C���ς݌^��񂪊܂܂�܂��B

#include "RLib/RLibPrecompile.h"

// TODO: ���̃t�@�C���ł͂Ȃ��ASTDAFX.H �ŕK�v��
// �ǉ��w�b�_�[���Q�Ƃ��Ă��������B

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
