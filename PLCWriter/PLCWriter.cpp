//-------------------------------------------------------------------------------
/*!
    @file	PLCWriter.cpp
    @brief	PLC書き込み処理
*/
//-------------------------------------------------------------------------------

#pragma warning( disable : 4996 )

#pragma region グローバル宣言
#include "framework.h"
#include "PLCWriter.h"

// 定数
#define WRITE_COUNT    7
#define TEXT_LEN       256

/// <summary>
/// エラー定義
/// </summary>
enum Error
{
    ERROR_NONE,
    ERROR_SOCKET_MAKE,
    ERROR_SOCKET_CONNECT,
    ERROR_SEND,
    ERROR_RECIEVE
};

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス

// このコード モジュールに含まれる関数の宣言を転送します:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
bool    CALLBACK    MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int                 fs_ExecWrite(HWND);
bool                fs_MakeSocket(SOCKET*);
bool                fs_SocketConnect(SOCKET*, const char*, USHORT);
void                fs_CloseSocket(SOCKET*);
bool                fs_WriteSend(SOCKET*, WORD*, int, int);
bool                fs_WriteRecieve(SOCKET*);
#pragma endregion


/// <summary>
/// アプリケーション エントリ ポイント
/// </summary>
/// <param name="hInstance">インスタンスハンドル</param>
/// <param name="hPrevInstance">0</param>
/// <param name="lpCmdLine">コマンドライン引数</param>
/// <param name="nCmdShow">初期ウィンドウサイズ</param>
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    hInst = hInstance;

    // ダイアログの名前と、プロシージャを指定してDialogBoxを開く
    int ret = DialogBox(hInst, MAKEINTRESOURCE(IDD_PLCWRITER_DIALOG), NULL, (DLGPROC)MainDlgProc);

    return ret;
}

#pragma region ダイアログ
/// <summary>
/// メインダイアログウィンドウプロシージャ
/// </summary>
/// <param name="hDlg">ウィンドウハンドル</param>
/// <param name="msg">メッセージ</param>
/// <param name="wp">WPARAM</param>
/// <param name="lp">LPARAM</param>
/// <returns>true：成功、false：失敗</returns>
bool CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    static WSADATA	WSAData = { 0 };
    int             iRet    = 0;

    switch (msg)
    {
    case WM_INITDIALOG:
        // ダイアログの初期化

        // WSAの初期化
        iRet = WSAStartup(MAKEWORD(2, 0), &WSAData);
        if (iRet != 0)
        {
            MessageBox(hDlg, TEXT("ダイアログの初期化に失敗しました。終了します。"), TEXT("実行結果"), MB_OK | MB_ICONERROR);
            EndDialog(hDlg, IDCANCEL);
        }
        break;
    
    case WM_DESTROY:
        // ダイアログの破棄

        // WSAをクリーン
        WSACleanup();
        PostQuitMessage(0);
        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:
        {
            TCHAR   tcMessage[TEXT_LEN] = { 0 };
            UINT    uiButtonIconFlag    = MB_OK | MB_ICONERROR;

            // 書込実行
            iRet = fs_ExecWrite(hDlg);

            // メッセージ表示
            switch (iRet)
            {
            case ERROR_NONE:
                _tcscpy(tcMessage, L"PLCの書き込み処理に成功しました。");
                uiButtonIconFlag = MB_OK | MB_ICONINFORMATION;
                break;
            case ERROR_SOCKET_MAKE:
                _tcscpy(tcMessage, L"ソケットの作成に失敗しました。");
                break;
            case ERROR_SOCKET_CONNECT:
                _tcscpy(tcMessage, L"ソケット接続に失敗しました。");
                break;
            case ERROR_SEND:
                _tcscpy(tcMessage, L"PLCへの書込コマンド送信に失敗しました。");
                break;
            case ERROR_RECIEVE:
                _tcscpy(tcMessage, L"PLCからの応答の受信に失敗しました。");
                break;
            }
            MessageBox(hDlg, tcMessage, TEXT("実行結果"), uiButtonIconFlag);
            return TRUE;
        }

        case IDCANCEL:
            // 閉じる
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        default:
            return DefWindowProc(hDlg, msg, wp, lp);
        }
        break;

    default:
        DefWindowProc(hDlg, msg, wp, lp);
        break;
    }

    return FALSE;
}
#pragma endregion

#pragma region 書込実行
/// <summary>
/// PLC書き込みを実行
/// </summary>
/// <param name="hDlg">ダイアログプロシージャ</param>
/// <returns>エラーコード</returns>
int fs_ExecWrite(HWND hDlg)
{
    const char* IP_ADDRESS              = "192.168.1.162";  // IPアドレス
    const WORD  PORT                    = 10000;            // ポート番号
    const int   START_ADDR              = 4000;             // 開始アドレス(D4000)

    TCHAR       tcBuffer[TEXT_LEN]      = { 0 };
    SOCKET      socket                  = INVALID_SOCKET;
    sockaddr_in addr                    = { 0 };
    bool        bRet                    = false;
    WORD        wWriteData[WRITE_COUNT] = { 0 };

    // EDITBOXから入力値を取得
    for (int i = 0; i < WRITE_COUNT; i++)
    {
        GetDlgItemText(hDlg, IDC_EDIT_D4000 + i, tcBuffer, TEXT_LEN);
        // テキストとして取得されるので、数値に変換
        wWriteData[i] = (WORD)_ttoi(tcBuffer);
    }

    // Socketを作成
    bRet = fs_MakeSocket(&socket);
    if (bRet == false)
    {
        // 失敗時はSocketをクローズして終了
        fs_CloseSocket(&socket);
        return ERROR_SOCKET_MAKE;
    }

    // Socketを接続
    bRet = fs_SocketConnect(&socket, IP_ADDRESS, PORT);
    if (bRet == false)
    {
        // 失敗時はSocketをクローズして終了
        fs_CloseSocket(&socket);
        return ERROR_SOCKET_CONNECT;
    }

    // PLCに書き込みコマンドを送信
    bRet = fs_WriteSend(&socket, wWriteData, START_ADDR, WRITE_COUNT);
    if (bRet == false)
    {
        // 失敗時はSocketをクローズして終了
        fs_CloseSocket(&socket);
        return ERROR_SEND;
    }

    // PLCから応答を受信
    bRet = fs_WriteRecieve(&socket);
    if (bRet == false)
    {
        // 失敗時はSocketをクローズして終了
        fs_CloseSocket(&socket);
        return ERROR_RECIEVE;
    }

    // Socketをクローズして終了
    fs_CloseSocket(&socket);
    return ERROR_NONE;
}
#pragma endregion

#pragma region Socket
/// <summary>
/// Socketを作成
/// </summary>
/// <param name="pSock">Socket</param>
/// <returns>true:成功 false:失敗</returns>
bool fs_MakeSocket(SOCKET* pSock)
{
    const int           iTimeout    = 500;   // タイムアウト（ミリ秒）
    struct sockaddr_in  addr        = { 0 };
    int                 iError      = 0;

    // SOCKETオブジェクトを設定
    *pSock = socket(AF_INET, SOCK_STREAM, 0);
    if (*pSock < 0) return false;

    // 送信タイムアウトを設定
    iError = setsockopt(*pSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iTimeout, sizeof(iTimeout));
    if (iError == SOCKET_ERROR) return false;
    // 受信タイムアウトを設定
    iError = setsockopt(*pSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iTimeout, sizeof(iTimeout));
    if (iError == SOCKET_ERROR) return false;

    // 送信元sockaddr_inオブジェクトを設定
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);

    // Socketをバインドする
    iError = bind(*pSock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if (iError == SOCKET_ERROR && *pSock != INVALID_SOCKET) return false;
    
    return true;
}

/// <summary>
/// Socket接続
/// </summary>
/// <param name="sock">Socket</param>
/// <param name="cIPAddress">IPアドレス</param>
/// <param name="port">ポート番号</param>
/// <returns>true:成功 false:失敗</returns>
bool fs_SocketConnect(SOCKET* pSock, const char* ipAddress, USHORT port)
{
    struct sockaddr_in  addr        = { 0 };
    unsigned long	    ulMode      = 1;        // 接続モード（0:同期型、1:非同期型）
    int                 iRet        = 0;
    int			        iErrorNo    = 0;
    fd_set			    fdsRead     = { 0 };
    fd_set			    fdsWrite    = { 0 };
    timeval             tvTimeout   = { 0 };

    // タイムアウトを有効にするため、非同期型にする
    iRet = ioctlsocket(*pSock, FIONBIO, &ulMode);
    if (iRet > 0) return false;

    // 送信先sockaddr_inオブジェクトを設定
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipAddress);
    addr.sin_port = htons(port);

    // 接続
    iRet = connect(*pSock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    iErrorNo = WSAGetLastError();
    // 非同期接続時はconnectの戻り値がSOCKET_ERROR、かつ、エラーコードがWSAEWOULDBLOCKであり、それ以外はエラーとする
    if (iRet != SOCKET_ERROR || iErrorNo != WSAEWOULDBLOCK)
    {
        return false;
    }

    // 同期型に戻す
    ulMode = 0;
    iRet = ioctlsocket(*pSock, FIONBIO, &ulMode);
    if (iRet > 0) return false;

    // selectで待機する
    tvTimeout.tv_sec = 2;
    tvTimeout.tv_usec = 0;
    FD_ZERO(&fdsRead);
    FD_ZERO(&fdsWrite);
    FD_SET(*pSock, &fdsRead);
    FD_SET(*pSock, &fdsWrite);
    
    iRet = select(1, &fdsRead, &fdsWrite, NULL, &tvTimeout);
    if (iRet == 0 || iRet == SOCKET_ERROR)
    {
        return false;
    }
    return true;
}

/// <summary>
/// Socketをクローズ
/// </summary>
/// <param name="pSock">Socket</param>
void fs_CloseSocket(SOCKET* pSock)
{
    shutdown(*pSock, SD_BOTH);
    closesocket(*pSock);
    *pSock = INVALID_SOCKET;
}
#pragma endregion

#pragma region コマンド通信
/// <summary>
/// 書き込みコマンドを送信
/// </summary>
/// <param name="writesocket">Socketオブジェクト</param>
/// <param name="data">書き込みデータ</param>
/// <param name="iStartAddr">書き込み先の先頭アドレス</param>
/// <param name="YousoNum">要素数</param>
bool fs_WriteSend(SOCKET* pSocket, WORD* data, int iStartAddr, int YousoNum)
{
    const int       SEND_MIN_LEN    = 21;
    unsigned char   pucSendCmd[5000]= { 0 };
    int             iRet            = 0;
    int             iDataLen        = 0;

    // 送信バッファを初期化
    memset(pucSendCmd, 0, sizeof(pucSendCmd));

    // 要求データ長を計算
    iDataLen = 12 + (YousoNum * 2);

    pucSendCmd[0] = 0x50;	    // サブヘッダ
    pucSendCmd[1] = 0x00;	    // サブヘッダ
    pucSendCmd[2] = 0x00;	    // ネットワーク番号
    pucSendCmd[3] = 0xFF;	    // PC番号
    pucSendCmd[4] = 0xFF;	    // 要求先ユニットI/O番号
    pucSendCmd[5] = 0x03;	    // 要求先ユニットI/O番号
    pucSendCmd[6] = (unsigned char)0; // 要求先ユニット局番号
    pucSendCmd[7] = (unsigned char)(iDataLen % 0x100); // 要求データ長
    pucSendCmd[8] = (unsigned char)(iDataLen / 0x100); // 要求データ長
    pucSendCmd[9] = 0x10;     // CPU監視タイマ
    pucSendCmd[10] = 0x00;	// CPU監視タイマ
    pucSendCmd[11] = 0x01;	/*--- コマンド ---------------------------------------------------------*/
    pucSendCmd[12] = 0x14;	/*--- コマンド ---------------------------------------------------------*/
    pucSendCmd[13] = 0x00;	// サブコマンド
    pucSendCmd[14] = 0x00;	// サブコマンド
    pucSendCmd[15] = (unsigned char)(iStartAddr % 0x100); // アドレス（LO）
    pucSendCmd[16] = (unsigned char)(iStartAddr / 0x100); // アドレス（MID)
    pucSendCmd[17] = 0x00;    // アドレス (High)
    pucSendCmd[18] = 0xA8;    // デバイスコード
    pucSendCmd[19] = (unsigned char)(YousoNum % 0x100);   // 要素数
    pucSendCmd[20] = (unsigned char)(YousoNum / 0x100);   // 要素数
    for (int i = 0; i < YousoNum; i++)
    {
        pucSendCmd[21 + i * 2] = (unsigned char)(data[i] % 0x100); //Lo
        pucSendCmd[22 + i * 2] = (unsigned char)(data[i] / 0x100); //High
    }
    iRet = send(*pSocket, (char*)pucSendCmd, SEND_MIN_LEN+YousoNum*2, 0);
    if (iRet != SEND_MIN_LEN + YousoNum * 2)
    {
        return false;
    }
    return true;
}

/// <summary>
/// 書き込みの応答を受信
/// </summary>
/// <param name="pSock">Socket</param>
/// <returns>true:成功 false:失敗</returns>
bool fs_WriteRecieve(SOCKET* pSock)
{
    const int       RECIEVE_BUFFER_SIZE = 11;
    const int       RETRY_TIMES         = 3;
    const int       RETRY_INTERVAL      = 500;
    const WORD      RECIEVE_DATA_SIZE   = 2;
    unsigned char   pucData[512]        = { 0 };
    int             iRet                = 0;
    WORD            wRecievedSize       = 0;

    for (int retry = 0; retry < RETRY_TIMES; retry++)
    {
        if (retry > 0) Sleep(RETRY_INTERVAL); 
        // 応答を受信
        iRet = recv(*pSock, (char*)pucData, RECIEVE_BUFFER_SIZE, 0);

        // 受信失敗時はリトライ
        if (iRet == 0 || iRet == SOCKET_ERROR) continue;

        memcpy(&wRecievedSize, &pucData[7], sizeof(WORD));
        // 受信サイズが不正
        if (wRecievedSize != RECIEVE_DATA_SIZE) break;

        // 応答結果がエラー
        if ((pucData[9] != 0x00) || (pucData[10] != 0x00)) break;

        // 正常終了
        return true;
    }
    // 異常終了
    return false;
}
#pragma endregion