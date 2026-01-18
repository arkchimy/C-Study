// LoginServer_Project.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
#include "LoginServer.h"
#include <iostream>

#include <conio.h>
#include <thread>
#include "MonitorData.h"

int main()
{

    CDump::SetHandlerDump();

    stWSAData wsa;

    wchar_t bindAddr[16];
    short bindPort;

    wchar_t ChatServerbindAddr[16];
    wchar_t Dummy1_ChatServerbindAddr[16];
    wchar_t Dummy2_ChatServerbindAddr[16];
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

        parser.GetValue(L"Dummy1_ChatServerIP", Dummy1_ChatServerbindAddr, 16);
        parser.GetValue(L"Dummy2_ChatServerIP", Dummy2_ChatServerbindAddr, 16);

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
        //parser.GetValue(L"ContentsRingBufferSize", ContentsRingBufferSize);
        parser.GetValue(L"EnCording", iEnCording);

        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    wchar_t buffer[100];


    StringCchPrintfW(buffer, 100, L"Profiler_%hs.txt", __DATE__);

    {
        //CTestServer::s_ContentsQsize = ContentsRingBufferSize;
        // 생성자에서 넘겨주는 것이 DB컨커런트
        CTestServer *LoginServer = new CTestServer(DBWorkerThreadCnt, iEnCording, DBContentsThreadCnt);
        CSystemLog::GetInstance()->SetDirectory(L"SystemLog");

        memcpy(LoginServer->ChatServerIP, ChatServerbindAddr, 32);
        memcpy(LoginServer->Dummy1_ChatServerIP, Dummy1_ChatServerbindAddr, 32);
        memcpy(LoginServer->Dummy2_ChatServerIP, Dummy2_ChatServerbindAddr, 32);
        LoginServer->ChatServerPort = ChatServerbindPort;

        LoginServer->Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions);
        ClientFunc();

        CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
        while (1)
        {
            if (GetAsyncKeyState(VK_ESCAPE))
            {
                CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");
                CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");
     
                LoginServer->bMonitorThreadOn = false;
                SetEvent(LoginServer->m_ServerOffEvent);
                LoginServer->Stop();

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
                if (ch == 'L' || ch == 'l')
                {
                    clsDeadLockManager::GetInstance()->CreateLogFile_TlsInfo();
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

        LoginServer->hMonitorThread.join();
        CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"ContentsThread_Terminate");
    }
}
