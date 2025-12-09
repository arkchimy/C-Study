// LoginServer_Project.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include "LoginServer.h"
#include <conio.h>
#include <thread>

#include "CrushDump_lib/CrushDump_lib.h"

int main()
{

    CDump::SetHandlerDump();

    st_WSAData wsa;

    wchar_t bindAddr[16];
    short bindPort;

    wchar_t ChatServerbindAddr[16];
    short ChatServerbindPort;

    int iZeroCopy;
    int iEnCording;
    int DBWorkerThreadCnt, DBContentsThreadCnt;
    int WorkerThreadCnt, ContentsThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;

    LINGER linger;
    int iRingBufferSize;
    int ContentsRingBufferSize;

    HRESULT hr;
    DWORD waitThread_Retval; // Waitfor 종료절차  반환값. 현재는 infinite

    {
        Parser parser;

        if (parser.LoadFile(L"Config.txt") == false)
            CSystemLog::GetInstance()->Log(L"ParserError.txt", en_LOG_LEVEL::ERROR_Mode, L"LoadFileError %d", GetLastError());
        parser.GetValue(L"ServerAddr", bindAddr, 16);
        parser.GetValue(L"ServerPort", bindPort);

        parser.GetValue(L"ChatServerIP", ChatServerbindAddr, 16);
        parser.GetValue(L"ChatServerPort", ChatServerbindPort);

   /*     WCHAR GameServerIP[16] = L"0.0.0.0";
        USHORT GameServerPort = 0;
        WCHAR ChatServerIP[16] = L"127.0.0.1";
        USHORT ChatServerPort = 6000;*/

        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", iZeroCopy);

        parser.GetValue(L"DBWorkerThreadCnt", DBWorkerThreadCnt);
        parser.GetValue(L"DBContentsThreadCnt", DBContentsThreadCnt);

        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
        parser.GetValue(L"ReduceThreadCount", reduceThreadCount);
        parser.GetValue(L"NoDelay", NoDelay);
        parser.GetValue(L"MaxSessions", maxSessions);

        parser.GetValue(L"ContentsThreadCnt", ContentsThreadCnt);
        parser.GetValue(L"RingBufferSize", iRingBufferSize);
        parser.GetValue(L"ContentsRingBufferSize", ContentsRingBufferSize);
        parser.GetValue(L"EnCording", iEnCording);

        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    wchar_t buffer[100];
    CSystemLog::GetInstance()->SetDirectory(L"SystemLog");

    StringCchPrintfW(buffer, 100, L"Profiler_%hs.txt", __DATE__);

    {
        CTestServer::s_ContentsQsize = ContentsRingBufferSize;
        // 생성자에서 넘겨주는 것이 DB컨커런트
        CTestServer *LoginServer = new CTestServer(DBWorkerThreadCnt, iEnCording, DBContentsThreadCnt);
        memcpy(LoginServer->ChatServerIP, ChatServerbindAddr, 32);
        LoginServer->ChatServerPort = ChatServerbindPort;

        LoginServer->Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions);
        CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
        while (1)
        {
            if (GetAsyncKeyState(VK_ESCAPE))
            {
                CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");
                CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");
                LoginServer->Stop();
                LoginServer->bMonitorThreadOn = false;

                SetEvent(LoginServer->m_ServerOffEvent);
                break;
            }
            if (_kbhit())
            {
                char ch = _getch();
                if (ch == 'A' || ch == 'a')
                {
                    CSystemLog::GetInstance()->SaveAsLog();
                    Profiler::SaveAsLog(buffer);
                }
                if (ch == 'D' || ch == 'd')
                {
                    Profiler::Reset();
                }
                if (ch == '1')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
                    printf("ERROR_Mode\n");
                }
                if (ch == '2')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_TargetMode);
                    printf("DEBUG_TargetMode\n");
                }
                if (ch == '3')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_Mode);
                    printf("DEBUG_Mode\n");
                }
                if (ch == 'P' || ch == 'p')
                {
                    Profiler::bOn = Profiler::bOn == true ? false : true;
                }
            }
        }

        waitThread_Retval = WaitForSingleObject(LoginServer->hMonitorThread, INFINITE);
        if (waitThread_Retval == WAIT_TIMEOUT)
        {
            // TODO : 시간 정한다면 어찌할지 정하기.
            __debugbreak();
        }
        CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"ContentsThread_Terminate");
    }
}
