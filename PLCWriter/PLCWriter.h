#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cassert>
using namespace std;

#include "resource.h"
#include "WinSock2.h"

// �萔
#define WRITE_COUNT    7
#define TEXT_LEN       256
#define BUFFER_LEN     512

/// <summary>
/// �G���[��`
/// </summary>
enum Error
{
    ERROR_NONE,
    ERROR_SOCKET_MAKE,
    ERROR_SOCKET_CONNECT,
    ERROR_SEND,
    ERROR_RECIEVE
};

// ���̃R�[�h ���W���[���Ɋ܂܂��֐��̐錾��]�����܂�:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
bool    CALLBACK    MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int                 fs_ExecWrite(HWND);
bool                fs_MakeSocket(SOCKET*);
bool                fs_SocketConnect(SOCKET*, const char*, USHORT);
void                fs_CloseSocket(SOCKET*);
bool                fs_WriteSend(SOCKET*, WORD*, int, int);
bool                fs_WriteRecieve(SOCKET*);