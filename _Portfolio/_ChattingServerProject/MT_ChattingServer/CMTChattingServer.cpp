
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")
#include <vector>


#include <thread>
#include <algorithm>


#pragma comment(lib, "Winmm.lib")

#include "CMTChattingServer.h"
#include "CNetworkLib/CNetworkLib.h"

#include <Pdh.h>
#include <stdio.h>

#pragma comment(lib, "Pdh.lib")

#include "./CNetworkLib/utility/CCpuUsage/CCpuUsage.h"

#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")

thread_local cpp_redis::client *client;
thread_local ull tls_ContentsQIdx; // ContentsThread 에서 사용하는 Q에  접근하기위한 Idx



void MsgTypePrint(WORD type, LONG64 val);

unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle = {server->m_ServerOffEvent};

    LONG64 UpdateTPS;
    LONG64 before_UpdateTPS = 0;

    LONG64 RecvTPS;
    LONG64 before_RecvTPS = 0;

    LONG64 *arrTPS;
    LONG64 *before_arrTPS;

    arrTPS = new LONG64[server->m_WorkThreadCnt + 1]; // Accept + 1
    before_arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));
    ZeroMemory(before_arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));

    LONG64 RecvMsgArr[en_PACKET_CS_CHAT__Max]{
        0,
    };
    LONG64 before_RecvMsgArr[en_PACKET_CS_CHAT__Max]{
        0,
    };

    // 얘는 signal 말고 전역변수로 끄자.

    timeBeginPeriod(1);

    DWORD currentTime = timeGetTime();
    DWORD nextTime; // 내가 목표로하는 이상적인 시간.
    nextTime = currentTime;

    ////ServerInfo

    int workthreadCnt = server->m_WorkThreadCnt;
    bool ZeroCopy = server->bZeroCopy;
    size_t MaxSessions = server->sessions_vec.size();
    int maxPlayers = server->m_maxPlayers;
    int bNoDelay = server->bNoDelay;

    char ProfilerFormat[2][30] = {
        "Profiler_Mode : Off\n",
        "Profiler_Mode : On\n"
    };

     // PDH 쿼리 핸들 생성
    PDH_HQUERY hQuery;
    PdhOpenQuery(NULL, NULL, &hQuery);

    PDH_HCOUNTER Process_PrivateByte;
    PdhAddCounter(hQuery, L"\\Process(MT_ChattingServer)\\Private Bytes", NULL, &Process_PrivateByte);

    PDH_HCOUNTER Process_NonpagedByte;
    PdhAddCounter(hQuery, L"\\Process(MT_ChattingServer)\\Pool Nonpaged Bytes", NULL, &Process_NonpagedByte);

    PDH_HCOUNTER Available_Byte;
    PdhAddCounter(hQuery, L"\\Memory\\Available MBytes", NULL, &Available_Byte);

    PDH_HCOUNTER Nonpaged_Byte;
    PdhAddCounter(hQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &Nonpaged_Byte);

    PDH_HCOUNTER hBytesRecv, hBytesSent, hBytesTotal;
    PDH_HCOUNTER hBytesRecv1, hBytesSent1, hBytesTotal1;
    PDH_HCOUNTER hBytesRecv2, hBytesSent2, hBytesTotal2;

    PDH_HCOUNTER hTcp4Retrans, hTcp4SegSent, hTcp4SegRecv;

    PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Received/sec"), 0, &hBytesRecv);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Received/sec"), 0, &hBytesRecv1);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Received/sec"), 0, &hBytesRecv2);

    PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Sent/sec"), 0, &hBytesSent);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Sent/sec"), 0, &hBytesSent1);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Sent/sec"), 0, &hBytesSent2);

    PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Total/sec"), 0, &hBytesTotal);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Total/sec"), 0, &hBytesTotal1);
    PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Total/sec"), 0, &hBytesTotal2);


    PdhAddCounter(hQuery, L"\\TCPv4\\Segments Retransmitted/sec", 0, &hTcp4Retrans);
    PdhAddCounter(hQuery, L"\\TCPv4\\Segments Sent/sec", 0, &hTcp4SegSent);
    PdhAddCounter(hQuery, L"\\TCPv4\\Segments Received/sec", 0, &hTcp4SegRecv);

    PdhCollectQueryData(hQuery);
    CCpuUsage CPUTime;

    {
        PDH_FMT_COUNTERVALUE Process_PrivateByteVal;
        PDH_FMT_COUNTERVALUE Process_Nonpaged_ByteVal;
        PDH_FMT_COUNTERVALUE Available_Byte_ByteVal;
        PDH_FMT_COUNTERVALUE Nonpaged_Byte_ByteVal;

        PDH_FMT_COUNTERVALUE recvVal[3];
        PDH_FMT_COUNTERVALUE sentVal[3];
        PDH_FMT_COUNTERVALUE totalVal[3];
        PDH_FMT_COUNTERVALUE vTcp4Retr, vTcp4Sent, vTcp4Recv;

        while (server->bMonitorThreadOn)
        {
            nextTime += 1000;
            {
                LONG64 old_UpdateTPS = server->m_UpdateTPS;
                UpdateTPS = old_UpdateTPS - before_UpdateTPS;
                before_UpdateTPS = old_UpdateTPS;
            }
            for (int i = 0; i <= workthreadCnt; i++)
            {
                LONG64 old_arrTPS = server->arrTPS[i];
                arrTPS[i] = old_arrTPS - before_arrTPS[i];
                before_arrTPS[i] = old_arrTPS;
            }
            printf(" ============================================ Config ============================================ \n");

            printf("%-25s : %10d  %-25s : %10lld\n", "WorkerThread Cnt", workthreadCnt, "SessionNum", server->GetSessionCount());
            printf("%-25s : %10d  %-25s : %10d \n", "ZeroCopy", ZeroCopy, "Nodelay", bNoDelay);
            printf("%-25s : %10zu  %-25s : %10d\n", "MaxSessions", MaxSessions, "MaxPlayers", maxPlayers);


            printf(" \n========================================= Server Runtime Status ====================================== \n");

            printf(" %-25s : %10llu  \n", "Total Accept", server->getTotalAccept());
            printf(" %-25s : %10lld  \n", "MyCustomMessage_Cnt", server->getNetworkMsgCount());
            printf(" %-25s : %10lld  %-25s : %10lld\n", "PrePlayer Count", server->GetprePlayer_hash(),
                   "Player Count", server->GetPlayerCount());

            printf(" %-25s : %10lld\n", "PacketPool", stTlsObjectPool<CMessage>::instance.m_TotalCount);
            printf(" %-25s : %10lld\n", "Total iDisconnectCount", server->iDisCounnectCount);
            printf(" %100s \n", ProfilerFormat[Profiler::bOn]);

            printf(" ============================================ Contents Thread TPS ========================================== \n");

            printf(" Accept TPS           : %lld\n", arrTPS[0]);
            printf(" Update TPS           : %lld\n", UpdateTPS);

            {
                LONG64 old_RecvTPS = server->m_RecvTPS;
                RecvTPS = old_RecvTPS - before_RecvTPS;
                before_RecvTPS = old_RecvTPS;
                printf(" Recv TPS : %lld\n", RecvTPS);
            }
            {
                LONG64 sum = 0;
                for (int i = 1; i <= server->m_WorkThreadCnt; i++)
                {
                    sum += arrTPS[i];
                    printf("%20s %10lld \n", "Send TPS :", arrTPS[i]);
                }
                printf(" ===================================================================================================== \n");
                printf("%35s %10lld\n", "Total Send TPS :", sum);
                printf(" ===================================================================================================== \n");
            }
            
            {
                LONG64 sum = 0;
                // Contents 정보 Print
                for (int idx = 0; idx < server->balanceVec.size(); idx++)
                {
                    printf("%15s %10s %05d  %10s %05lld\n",
                           "Contetent",
                           "Session :", server->balanceVec[idx].second,
                           "UpdateMessage_Queue", server->m_CotentsQ_vec[idx].m_size);
                }
            }
            printf(" ===================================================================================================== \n");

            // 메세지 별
            for (int i = 3; i < en_PACKET_CS_CHAT__Max - 1; i++)
            {
                LONG64 old_RecvMsg = server->m_RecvMsgArr[i];
                RecvMsgArr[i] = old_RecvMsg - before_RecvMsgArr[i];
                before_RecvMsgArr[i] = old_RecvMsg;
                MsgTypePrint(i, RecvMsgArr[i]);
            }

            {
                // 1초마다 갱신
                PdhCollectQueryData(hQuery);

                CPUTime.UpdateCpuTime();

                wprintf(L" ============================================ CPU Useage ============================================ \n");

                wprintf(L" [ Total ]T:%03.2f U : %03.2f  K : %03.2f \t", CPUTime.ProcessorTotal(), CPUTime.ProcessorKernel(), CPUTime.ProcessorUser());
                wprintf(L" [ Process ] T:%03.2f U : %03.2f  K : %03.2f   \n", CPUTime.ProcessTotal(), CPUTime.ProcessKernel(), CPUTime.ProcessUser());
                wprintf(L"====================================================================================================\n");

                // 갱신 데이터 얻음
                //PDH_FMT_COUNTERVALUE counterVal;
                PdhGetFormattedCounterValue(Process_PrivateByte, PDH_FMT_LARGE, NULL, &Process_PrivateByteVal);
                wprintf(L"Process_PrivateByte : %lld Byte\n", Process_PrivateByteVal.largeValue);

                PdhGetFormattedCounterValue(Process_NonpagedByte, PDH_FMT_LARGE, NULL, &Process_Nonpaged_ByteVal);
                wprintf(L"Process_Nonpaged_Byte :  %lld Byte\n", Process_Nonpaged_ByteVal.largeValue);

                PdhGetFormattedCounterValue(Available_Byte, PDH_FMT_LARGE, NULL, &Available_Byte_ByteVal);
                wprintf(L"Available_Byte :  %lld Byte\n", Available_Byte_ByteVal.largeValue);

                PdhGetFormattedCounterValue(Nonpaged_Byte, PDH_FMT_LARGE, NULL, &Nonpaged_Byte_ByteVal);
                wprintf(L"Nonpaged_Byte_ByteVal : %lld Byte\n", Nonpaged_Byte_ByteVal.largeValue);
            }
            // NetWork 
            {
                PdhGetFormattedCounterValue(hBytesRecv, PDH_FMT_DOUBLE, NULL, &recvVal[0]);
                PdhGetFormattedCounterValue(hBytesRecv1, PDH_FMT_DOUBLE, NULL, &recvVal[1]);
                PdhGetFormattedCounterValue(hBytesRecv2, PDH_FMT_DOUBLE, NULL, &recvVal[2]);

                PdhGetFormattedCounterValue(hBytesSent, PDH_FMT_DOUBLE, NULL, &sentVal[0]);
                PdhGetFormattedCounterValue(hBytesSent1, PDH_FMT_DOUBLE, NULL, &sentVal[1]);
                PdhGetFormattedCounterValue(hBytesSent2, PDH_FMT_DOUBLE, NULL, &sentVal[2]);

                PdhGetFormattedCounterValue(hBytesTotal, PDH_FMT_DOUBLE, NULL, &totalVal[0]);
                PdhGetFormattedCounterValue(hBytesTotal1, PDH_FMT_DOUBLE, NULL, &totalVal[1]);
                PdhGetFormattedCounterValue(hBytesTotal2, PDH_FMT_DOUBLE, NULL, &totalVal[2]);

                PdhGetFormattedCounterValue(hTcp4Retrans, PDH_FMT_DOUBLE, NULL, &vTcp4Retr);
                PdhGetFormattedCounterValue(hTcp4SegSent, PDH_FMT_DOUBLE, NULL, &vTcp4Sent);
                PdhGetFormattedCounterValue(hTcp4SegRecv, PDH_FMT_DOUBLE, NULL, &vTcp4Recv);

                 double tcp4RetrRatio = 0.0;
                if (vTcp4Sent.doubleValue > 0.0)
                    tcp4RetrRatio = (vTcp4Retr.doubleValue / vTcp4Sent.doubleValue) * 100.0;


                wprintf(L"\n ============================================ Network Usage (Bytes/sec) ============================================ \n");
                wprintf(L"%20s %15s %15s %15s\n",
                        L"Adapter",
                        L"Recv(B/s)",
                        L"Sent(B/s)",
                        L"Total(B/s)");
                wprintf(L"--------------------------------------------------------------------------\n");

                wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                        L"Realtek PCIe GbE Family Controller",
                        recvVal[0].doubleValue,
                        sentVal[0].doubleValue,
                        totalVal[0].doubleValue);

                wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                        L"Intel[R] I210 Gigabit Network Connection",
                        recvVal[1].doubleValue,
                        sentVal[1].doubleValue,
                        totalVal[1].doubleValue);

                wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                        L"Intel[R] I210 Gigabit Network Connection _2",
                        recvVal[2].doubleValue,
                        sentVal[2].doubleValue,
                        totalVal[2].doubleValue);

                wprintf(L"\n ============================================ TCP Retransmission ============================================ \n");
                wprintf(L"TCPv4 Segments Sent/sec          : %.2f\n", vTcp4Sent.doubleValue);
                wprintf(L"TCPv4 Segments Retransmitted/sec : %.2f\n", vTcp4Retr.doubleValue);
                wprintf(L"TCPv4 Retrans Ratio              : %.2f %%\n", tcp4RetrRatio);

                wprintf(L" ============================================================================================================== \n");
            }

            currentTime = timeGetTime();
            if (nextTime > currentTime)
                Sleep(nextTime - currentTime);
        }
    }
    //종료 절차.

    DWORD waitThread_Retval;
    std::wstring Accept_str(L"AcceptThread Shutdown in progress");
    std::wstring Worker_str(L"WorkerThread Shutdown in progress");
    std::wstring Contents_str(L"ContentsThread Shutdown in progress");

    std::wstring Accept_str2(L"AcceptThread Shutdown complete.");
    std::wstring Worker_str2(L"WorkerThread");
    std::wstring Contents_str2(L"ContentsThread");

    wchar_t dot_str[][11] = {
        L".",
        L"..",
        L"...",
        L"....",
        L".....",
        L"......",
        L".......",
        L"........",
        L".........",
        L"..........",
    };
    bool bOn = true;
    while (bOn)
    {
        static DWORD Chance = 0;
        if (Chance % 10000 == 0)
            bOn = false;
   
        Chance++;

        nextTime += 1000;
        printf(" =========================Waiting for Threads to terminate...=========================\n");
        // AcceptThread 종료
        {

            waitThread_Retval = WaitForSingleObject(server->m_hAccept.native_handle(), 0);
            if (waitThread_Retval == WAIT_TIMEOUT)
            {
                wprintf(L"\t%30s\n", (Accept_str + dot_str[Chance % 10]).c_str());
                bOn = true;
            }
            else
            {
                wprintf(L"\t%30s\n", (Accept_str2).c_str());
            }
            
        }
        // WorkerThread 종료
        for (int i = 0; i < server->m_WorkThreadCnt; i++)
        {
            waitThread_Retval = WaitForSingleObject(server->m_hWorkerThread[i].native_handle(), 0);
            if (waitThread_Retval == WAIT_TIMEOUT)
            {
                wprintf(L"\t%30s\n", (Worker_str + std::to_wstring(i) + dot_str[Chance % 10]).c_str());
                bOn = true;
            }
            else
            {
                wprintf(L"\t%30s :  %20s\n", (Worker_str2 + std::to_wstring(i)).c_str(), L"\tShutdown complete.");
            }
        }

        // ContentsThread 종료
        for (int i = 0; i < server->m_ContentsThreadCnt; i++)
        {
            waitThread_Retval = WaitForSingleObject(server->hContentsThread_vec[i].native_handle(), 0);
            if (waitThread_Retval == WAIT_TIMEOUT)
            {
                wprintf(L"%30s\n", (Contents_str + std::to_wstring(i) + dot_str[Chance % 10]).c_str());
                bOn = true;
            }
            else
            {
                wprintf(L"\t %30s :  %20s\n", (Contents_str2 + std::to_wstring(i)).c_str() , L"\tShutdown complete.");
            }
        }
        printf(" =========================  Wait for Job Process terminate  =========================\n");

        LONG64 sum = 0;
        for (int idx = 0; idx < server->balanceVec.size(); idx++)
        {
            sum += server->balanceVec[idx].second;
            printf("%15s %10s %05d  %10s %05lld\n",
                   "Contetent",
                   "Session :", server->balanceVec[idx].second,
                   "UpdateMessage_Queue", server->m_CotentsQ_vec[idx].m_size);
        }
        if (sum != 0)
            bOn = true;

        currentTime = timeGetTime();
        if (nextTime > currentTime)
            Sleep(nextTime - currentTime);
        printf(" ===================================================================================\n");
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"MonitorThread Terminated %d", 0);
    return 0;
}
//arg[0] pServer
//arg[1] pContentsQ

void ContentsThread(void *arg)
{
    clsDeadLockManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo);

    DWORD hSignalIdx;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    CLockFreeQueue<CMessage*> *CotentsQ;
    HANDLE local_ContentsEvent;

    cpp_redis::client a;
    client = &a;

    client->connect(server->RedisIpAddress, 6379);


    {
        tls_ContentsQIdx = InterlockedIncrement(&server->m_ContentsThreadIdX);
        CotentsQ = &server->m_CotentsQ_vec[tls_ContentsQIdx];
        local_ContentsEvent = server->m_ContentsQMap[CotentsQ];
    }

    HANDLE hWaitHandle[2] = {local_ContentsEvent, server->m_ServerOffEvent};

    // bool 이 좋아보임.
    {
        std::wstring ContentsThreadName;
        WCHAR* DS;
        GetThreadDescription(GetCurrentThread(), &DS);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       DS);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       DS);
    }
    ringBufferSize ContentsUseSize;

    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
      
        {
            Profiler profile(L"Update");
            server->Update();
        }
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
        {
            break;
        }

        ContentsUseSize = CotentsQ->m_size;
        server->m_UpdateMessage_Queue = (LONG64)(ContentsUseSize / 8);

        server->prePlayer_hash_size = server->prePlayer_hash.size();
        server->AccountNo_hash_size = server->AccountNo_hash.size();
        server->SessionID_hash_size = server->SessionID_hash.size();
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"ContentsThread Terminated %d", 0);

}

void CTestServer::REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *ID, WCHAR *Nickname, WCHAR *SessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, WORD wVectorLen)
{
    stPlayer *player;

    // 옳바른 연결인지는 Token에 의존.
    // Alloc을 받았다면 prePlayer_hash에 추가되어있을 것이다.
    auto prePlayeriter = prePlayer_hash.find(SessionID);

    if (prePlayeriter == prePlayer_hash.end())
    {
        // 없다는 것은 내가 만든 절차를 따르지않았음을 의미.

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %05lld %12s %05llu ",
                                       L"LoginError - prePlayer_hash not found : ", AccountNo,
                                       L"현재들어온ID:", SessionID);

        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }
    player = prePlayeriter->second;

    if (player->m_State != enPlayerState::Session)
    {
        // 로그인 패킷을 여러번 보낸 경우.
        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %05lld %12s %05lld",
                                       L"LoginError - state not equle Session : ", AccountNo,
                                       L"현재들어온ID:", SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }

    // 중복 접속 문제

    if (AccountNo_hash.find(AccountNo) != AccountNo_hash.end())
    {
        // Todo : 완전히 보낸 것을 확인한 후에 연결을 끊는 상황도 만들어줘야함.
        // 일단은 안만듬
        //Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);

        // 중복 로그인 이면 기존에 있던 User만 끊기. 
        // =>  RST유실 혹은 지연 삭제로 인함.

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %05lld %12s %05lld %12s %05llu ",
                                       L"중복접속문제Account : ", AccountNo,
                                       L"현재들어온ID:", SessionID,
                                       L"현재들어온ID:", AccountNo_hash[AccountNo]->m_sessionID);

        //stTlsObjectPool<CMessage>::Release(msg);
        //Disconnect(SessionID);
        // 이 경우때문에 결국 Player에 SessionID가 필요함.
        Disconnect(AccountNo_hash[AccountNo]->m_sessionID);
        //return;
    }
    std::vector<std::pair<DWORD, int>>::iterator iter = std::min_element(balanceVec.begin(), balanceVec.end(),
                                  [](const std::pair<DWORD, int> &a, const std::pair<DWORD, int> &b)
                                {
                                    return a.second < b.second; // value 기준으로 비교
                                });
    DWORD idx = iter->first;
    iter->second++;

    player->m_State = enPlayerState::Player;
    InterlockedExchange64((LONG64*) & player->m_AccountNo, AccountNo);
    memcpy(player->m_ID, ID, 20);
    memcpy(player->m_Nickname, Nickname, 20);
    memcpy(player->m_SessionKey, SessionKey, 64);

    memcpy(player->m_SessionKey, SessionKey, sizeof(player->m_SessionKey));

    SessionID_hash[SessionID] = player;
    AccountNo_hash[AccountNo] = player;

    prePlayer_hash.erase(SessionID);



    m_TotalPlayers++; // Player 카운트
    m_prePlayerCount--;


    _InterlockedExchange(&player->m_ContentsQIdx, idx);

    {
        // Login응답.
        // redis에서 읽기, 가져오고 token을 비교 같다면 
        std::string key = std::to_string(AccountNo);
        auto future = client->get(key.c_str());
        client->sync_commit();
        cpp_redis::reply reply = future.get();

        if (reply.is_null())
        {
            printf("AccountNo %lld not found in redis\n", AccountNo);
            __debugbreak();
            return;
        }
        std::string sessionKey = reply.as_string();

        char SessionKeyA[66];
        memcpy(SessionKeyA, SessionKey, 64);
        SessionKeyA[64] = '\0';
        SessionKeyA[65] = '\0';

        key = std::to_string(AccountNo);

        if (sessionKey.compare(SessionKeyA) != 0)
            __debugbreak();
        Proxy::RES_LOGIN(SessionID, msg, true, AccountNo);

        InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_LOGIN]);
    }
    InterlockedExchange(&player->m_Timer, timeGetTime());
}
void CTestServer::REQ_SECTOR_MOVE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD SectorX, WORD SectorY, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, WORD wVectorLen)
{
    stPlayer *player;
    WORD beforeX, beforeY;

    if (SectorX >= m_sectorManager._MaxX || SectorY >= m_sectorManager._MaxY)
    {
        static bool bOn = false;
        if (bOn == false)
        {
            bOn = true;
            CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %20s %05lld %12s %05llu %12s %05llu %12s %05llu ",
                                           L"REQ_SECTOR_MOVE MessageData  : ",
                                           L"AccountNo", AccountNo,
                                           L"SectorX", SectorX,
                                           L"SectorY", SectorY,
                                           L"현재들어온ID:", SessionID);
        }

        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);

        return;
    }

    {
        std::shared_lock<SharedMutex> lock(srw_SessionID_Hash);
        auto iter = SessionID_hash.find(SessionID);
        if (iter == SessionID_hash.end())
        {
            // Attack : Login을 안하고 들어옴
            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %20s %20s %05lld  ",
                                               L" REQ_SECTOR_MOVE ",
                                               L"Not Login Session REQ_MESSAGE",
                                               L"DisConnect SessionId :",
                                               SessionID);
            }
            stTlsObjectPool<CMessage>::Release(msg);
            return;
        }
        player = iter->second;

        if (player->iSectorX >= m_sectorManager._MaxX || player->iSectorY >= m_sectorManager._MaxY )
        {
            // player 초기화를 안한듯
            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %20s %05lld %12s %05llu %12s %05llu %12s %05llu ",
                                               L"REQ_SECTOR_MOVE PlayerData  : ",
                                               L"AccountNo", AccountNo,
                                               L"SectorX", SectorX,
                                               L"SectorY", SectorY,
                                               L"현재들어온ID:", SessionID);
            }
            Disconnect(SessionID);
            stTlsObjectPool<CMessage>::Release(msg);

            return;
        }

        beforeX = player->iSectorX;
        beforeY = player->iSectorY;

        player->iSectorX = SectorX;
        player->iSectorY = SectorY;

    }

    if (player->m_AccountNo != AccountNo)
    {
        //발견 
        static bool bOn = false;
        if (bOn == false)
        {
            bOn = true;
            CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %12s %05llu %12s %05lld %12s %05lld ",
                                           L"REQ_SECTOR_MOVE m_AccountNo != AccountNo : ",
                                           L"현재들어온ID:", SessionID,
                                           L"현재들어온Account:", AccountNo,
                                           L"player Account:", player->m_AccountNo);
        }


        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    {
        std::lock_guard<SharedMutex> lock(srw_Sectors[beforeY][beforeX]);
        g_Sector[beforeY][beforeX].erase(SessionID);
    }



    {
        std::lock_guard<SharedMutex> lock(srw_Sectors[SectorY][SectorX]);
        g_Sector[SectorY][SectorX].insert(SessionID);
    }

    Proxy::RES_SECTOR_MOVE(SessionID, msg, AccountNo, SectorX, SectorY);
    InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_SECTOR_MOVE]);

}
void CTestServer::REQ_MESSAGE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD MessageLen, WCHAR *MessageBuffer, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, WORD wVectorLen)
{
    stPlayer *player;
    int SectorX;
    int SectorY;

    {
        // SessionID_HashLock 획득.
        std::shared_lock<SharedMutex> SessionID_Hashlock(srw_SessionID_Hash);
        auto iter = SessionID_hash.find(SessionID);

        if (iter == SessionID_hash.end())
        {
            // Attack : Login을 안하고 들어옴
            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %20s %20s %05lld  ",
                                               L" REQ_MESSAGE ",
                                               L"Not Login Session REQ_MESSAGE",
                                               L"DisConnect SessionId :",
                                               SessionID);
            }
            Disconnect(SessionID);
            stTlsObjectPool<CMessage>::Release(msg);

            return;
        }

        player = iter->second;

        SectorX = player->iSectorX;
        SectorY = player->iSectorY;

        if (player->m_AccountNo != AccountNo)
        {

            CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %12s %05llu %12s %05lld  %12s %05lld ",
                                           L"REQ_MESSAGE m_AccountNo != AccountNo : ",
                                           L"현재들어온ID:", SessionID,
                                           L"현재들어온Account:", AccountNo,
                                           L"player Account:", player->m_AccountNo);

            Disconnect(SessionID);
            stTlsObjectPool<CMessage>::Release(msg);
            return;
        }
        // TODO : Broad Cast  방식 수정하기.
        std::vector<ull> sendID;

        st_Sector_Around AroundSectors;
        m_sectorManager.GetSectorAround(SectorX, SectorY, AroundSectors);

        size_t vectorReserverSize = 0;

        std::vector<std::shared_lock<SharedMutex>> SectorAround_locks;
        SectorAround_locks.reserve(AroundSectors.Around.size());

        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            SectorAround_locks.emplace_back(srw_Sectors[targetSector._iY][targetSector._iX]);
        }
      
        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            vectorReserverSize += g_Sector[targetSector._iY][targetSector._iX].size();
        }
        sendID.reserve(vectorReserverSize);

        {
            Profiler profile(L"Broad_Loop");
            // InterlockedExchange64(&msg->iUseCnt, vectorReserverSize);  BroadCast 내부에서 실행.
            for (const st_Sector_Pos targetSector : AroundSectors.Around)
            {
                for (ull id : g_Sector[targetSector._iY][targetSector._iX])
                {
                    // TODO : 이 부분 SessionID_hash 제거 실험.
                    if (SessionID_hash.find(id) != SessionID_hash.end())
                    {
                        sendID.push_back(id); // BroadCast시에만 사용이지만 성능측정을 위해 납둠.
                        //Unicast(id,msg);
                        InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
                    }
                }
            }
            Proxy::RES_MESSAGE(SessionID, msg, AccountNo, player->m_ID, player->m_Nickname, MessageLen,
                               MessageBuffer, en_PACKET_CS_CHAT_RES_MESSAGE, true, &sendID, sendID.size());
        }
  
        
    }
  
}

void CTestServer::HEARTBEAT(ull SessionID, CMessage *msg, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, WORD wVectorLen)
{
    stPlayer *player;

    std::shared_lock<SharedMutex> SessionID_Hashlock(srw_SessionID_Hash);
    auto iter = SessionID_hash.find(SessionID);

    if (iter == SessionID_hash.end())
    {

        stTlsObjectPool<CMessage>::Release(msg);

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                        L"%-20s %12s %05llu  ",
                                        L"HEARTBEAT SessionID_hash not Found : ",
                                        L"현재들어온ID:", SessionID);

        Disconnect(SessionID);
        return;
    }

    player = iter->second;

    InterlockedExchange(&player->m_Timer, timeGetTime());


    stTlsObjectPool<CMessage>::Release(msg);
}

void CTestServer::AllocPlayer(CMessage *msg)
{
    // AcceptThread 에서 삽입한 메세지.

    stPlayer *player;
    ull SessionID;

    InterlockedDecrement64(&m_AllocMsgCount);
    SessionID = msg->ownerID;
    // 옳바른 연결인지는 Token에 의존.

    if (prePlayer_hash.find(SessionID) != prePlayer_hash.end())
    {
        // Attack : LoginPacket 중복으로 보냄
        static bool bOn = false;
        if (bOn == false)
        {
            bOn = true;
            CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %20s %20s %05lld  ",
                                           L"AllocPlayer ",
                                           L"Login Packet replay attack",
                                           L"DisConnect SessionId :",
                                           SessionID);
        }
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }


    player = (stPlayer *)player_pool.Alloc();
    if (player == nullptr)
    {
        // Player가 가득 참.
        // TODO : 이 경우에는 끊기보다는 유예를 둬야하나?

        stTlsObjectPool<CMessage>::Release(msg);

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %d %12s %05llu ",
                                       L"Player is Fulled : ", 0,
                                       L"현재들어온ID:", SessionID);

        Disconnect(SessionID);
        //SessionUnLock(SessionID);
        return;
    }
    // 이때 할당.
    player->Initalize();

    prePlayer_hash[SessionID] = player;
    // prePlayer_hash 는 Login을 기다리는 Session임.
    // LoginPacket을 받았다면 ;

    InterlockedExchange(&player->m_Timer, timeGetTime());

    player->m_sessionID = SessionID;
    player->m_State = enPlayerState::Session;

    msg->GetData(player->m_ipAddress, 16);
    *msg >> player->m_port;

    m_prePlayerCount++;
    _InterlockedDecrement64(&m_NetworkMsgCount);
    stTlsObjectPool<CMessage>::Release(msg);

}

void CTestServer::DeletePlayer(CMessage *msg)
{
    // TODO : 이 구현이 TestServer에 있는게 맞는가?
    // Player를 다루는 모든 자료구조에서 해당 Player를 제거.

    ull SessionID;
    stPlayer *player;
    SessionID = msg->ownerID;

    auto prePlayeriter = prePlayer_hash.find(SessionID);
    if (prePlayeriter == prePlayer_hash.end())
    {
        // Player라면
        auto iter = SessionID_hash.find(SessionID);
        if (iter == SessionID_hash.end())
        {
            // Attack : 내 만든 메세지일 경우 단 한번만 이 메세지가 오겠지만,
            // 근데 만일 공격 패턴에서 DeletePlayer의 Type을 맞춘다면?
            // 이쪽 루틴을 탈 것.

            // 확인을 위해서는 같은 SessionID로  두번이상의 DeletePlayer가 발생 하였는지 알 수 있으면 됨.

            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L" %-20s %20s %05lld  ",
                                               L" DeletePlayer ",
                                               L" Delete Packet replay attack ",
                                               SessionID);
            }

        }
        else
        {
            player = iter->second;
            SessionID_hash.erase(iter);

            if (AccountNo_hash.find(player->m_AccountNo) == AccountNo_hash.end())
            {
                //  Attack이 아니라 있을 수 없는 상황.
                CSystemLog::GetInstance()->Log(L"CriticalError", en_LOG_LEVEL::ERROR_Mode,
                                               L" %-20s %20s %05lld  ",
                                               L" DeletePlayer ",
                                               L" AccountNo_hash not Found ",
                                               SessionID);
   
            }
            auto it = AccountNo_hash.find(player->m_AccountNo);
            if (it != AccountNo_hash.end() && it->second == player)
                AccountNo_hash.erase(it);
            m_TotalPlayers--;

            if (player->iSectorX >= m_sectorManager._MaxX || player->iSectorY >= m_sectorManager._MaxY)
            {
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L" %-20s %20s  %20s 05d %20s %05d  ",
                                                   L" DeletePlayer ",
                                                   L" Player Sector is not initialized ",
                                                   L" Player iSectorX ",
                                                   player->iSectorX,
                                                   L" Player iSectorY ",
                                                   player->iSectorY);
                }


                Disconnect(SessionID);
                stTlsObjectPool<CMessage>::Release(msg);

                return;
            }
            {
                std::lock_guard<SharedMutex> SessionID_Hashlock(srw_Sectors[player->iSectorY][player->iSectorX]);
                g_Sector[player->iSectorY][player->iSectorX].erase(SessionID);
            }

            player->m_State = enPlayerState::DisConnect;


            for (auto& element : balanceVec)
            {
                if (element.first == player->m_ContentsQIdx)
                {
                    element.second--;
                    break;
                }
            }
            player_pool.Release(player);
        }
    
    }
    else
    {
        // Accept 이후  DeletePlayer가 LoginPacket보다 먼저 옴.
        player = prePlayeriter->second;
        prePlayer_hash.erase(SessionID);
        m_prePlayerCount--;

        player->m_State = enPlayerState::DisConnect;

        player_pool.Release(player);
    }
    _InterlockedDecrement64(&m_NetworkMsgCount);
    stTlsObjectPool<CMessage>::Release(msg);

}

CTestServer::CTestServer(int ContentsThreadCnt, int iEncording)
    : CLanServer(iEncording), m_ContentsThreadCnt(ContentsThreadCnt), m_RecvTPS(0), m_UpdateTPS(0), m_UpdateMessage_Queue(0), hBalanceThread(0)
{
    HRESULT hr;
    player_pool.Initalize(m_maxPlayers);
    player_pool.Limite_Lock(); // Pool이 더 이상 늘어나지않음.

    // Sector 정보 캐싱.
    m_sectorManager.Initalize();

    // BalanceThread를 위한 정보 초기화.
    {

        m_ServerOffEvent = CreateEvent(nullptr, true, false, nullptr);
        hBalanceEvent = CreateEvent(nullptr, false, false, nullptr);
    }

    pBalanceThread = std::thread(&CTestServer::BalanceThread, this);
    hr = SetThreadDescription(pBalanceThread.native_handle(), L"\tBalanceThread");
    RT_ASSERT(!FAILED(hr));

    //    std::map<CRingBuffer *, SRWLOCK> m_SrwMap;
    //    각 메세지 Q에 대해서 SRWLOCK을 획득하는 자료구조 초기화 하기.
    {
        balanceVec.reserve(ContentsThreadCnt);

        m_CotentsQ_vec.reserve(ContentsThreadCnt);
        hContentsThread_vec.reserve(ContentsThreadCnt);

        for (int i = 0; i < ContentsThreadCnt; i++)
        {
            balanceVec.emplace_back(i, 0);
            m_CotentsQ_vec.emplace_back();
            m_ContentsQMap[&m_CotentsQ_vec[i]] = CreateEvent(nullptr, false, false, nullptr);
        }

    }

    // ContentsThread의 생성
    hContentsThread_vec.resize(m_ContentsThreadCnt);

    for (int i = 0; i < m_ContentsThreadCnt; i++)
    {
        std::wstring ContentsThreadName = L"\tContentsThread" + std::to_wstring(i);

        hContentsThread_vec[i] = std::move(std::thread(ContentsThread, this));

        RT_ASSERT(hContentsThread_vec[i].native_handle() != nullptr);

        hr = SetThreadDescription(hContentsThread_vec[i].native_handle(), ContentsThreadName.c_str());
        RT_ASSERT(!FAILED(hr));
    }


}


CTestServer::~CTestServer()
{
    pBalanceThread.join(); // Balance

    for (int i = 0; i < m_ContentsThreadCnt; i++)
    {
        hContentsThread_vec[i].join();
    }
}

void CTestServer::Update()
{
    CMessage *msg;
    ull l_sessionID;

    WORD wType;

    CLockFreeQueue<CMessage*>* CotentsQ = &m_CotentsQ_vec[tls_ContentsQIdx];
    // msg  크기 메세지 하나에 8Byte
    while (CotentsQ->m_size != 0)
    {
        stPlayer *player = nullptr;


        {
            Profiler profile(L"Contents_DeQ");
            CotentsQ->Pop(msg);
        }
        l_sessionID = msg->ownerID;

        try
        {
            *msg >> wType;
        }
        catch (const MessageException &e)
        {
            switch (e.type())
            {
            case MessageException::ErrorType::HasNotData:
            {
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s %20s %05d  ",
                                                   L" msg >> Data  Faild ",
                                                   L"wType", wType);
                }
                break;
            }
            case MessageException::ErrorType::NotEnoughSpace:
            {
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s %20s %05d  ",
                                                   L"NotEnoughSpace  : ",
                                                   L"wType", wType);
                }
            }
                msg->HexLog(CMessage::en_Tag::_ERROR, L"Attack.txt");
                break;
            }
            stTlsObjectPool<CMessage>::Release(msg);
            Disconnect(l_sessionID);
            continue;
        }
        // LoginPacket이 안오는 경우가 존재하지않음.
        switch (wType)
        {
        // 현재 미 사용중
        case en_PACKET_Player_Alloc:
        case en_PACKET_Player_Delete:
        case en_PACKET_CS_CHAT_REQ_LOGIN:
            stTlsObjectPool<CMessage>::Release(msg);
            Disconnect(l_sessionID);
            continue;
        }

        { //  Client Message
            {

                std::shared_lock<SharedMutex> SessionID_HashLock(srw_SessionID_Hash);

                if (SessionID_hash.find(l_sessionID) != SessionID_hash.end())
                    player = SessionID_hash[l_sessionID];
                else
                {
                    stTlsObjectPool<CMessage>::Release(msg);
                    continue;
                }
            }
            InterlockedExchange(&player->m_Timer, timeGetTime());
            {
                Profiler profiler(L"PacketProc");
                if (PacketProc(l_sessionID, msg, wType) == false)
                {
                    stTlsObjectPool<CMessage>::Release(msg);
                    Disconnect(l_sessionID);
                }
                else
                    InterlockedIncrement64(&m_RecvMsgArr[wType]);
            }
            
        }
        
        InterlockedIncrement64(&m_UpdateTPS);
    }

}

void CTestServer::BalanceThread()
{
    clsDeadLockManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo);
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"BalanceThread");
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"BalanceThread");
    }

    DWORD hSignalIdx;
    ringBufferSize ContentsUseSize;
    // m_ServerOffEvent 는 ESCAPE 누를 시 호출 
    HANDLE hWaitHandle[2] = {hBalanceEvent, m_ServerOffEvent};

    cpp_redis::client a;
    client = &a;

    client->connect(RedisIpAddress, 6379);

    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        {
            Profiler BalanceUpdateprofile(L"BalanceUpdate");
            BalanceUpdate();
        }

        ContentsUseSize = m_BalanceQ.m_size;

        //모니터링 변수 초기화.
        {
            prePlayer_hash_size = prePlayer_hash.size();
            AccountNo_hash_size = AccountNo_hash.size();
            SessionID_hash_size = SessionID_hash.size();
        }

        if (hSignalIdx - WAIT_OBJECT_0 == 1 && prePlayer_hash_size == 0 && AccountNo_hash_size == 0 && SessionID_hash_size == 0 && ContentsUseSize == 0)
        {
            // 종료 메세지. 워커쓰레드 수 만큼
            SignalOnForStop();
            break;
        }
        else if (hSignalIdx - WAIT_OBJECT_0 == 1)
        {
            // 아직 유저가 전부 나가지않음.
            CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"BalanceThread wait Player Exit %lld  %lld %lld",
                                           prePlayer_hash_size,
                                           AccountNo_hash_size,
                                           SessionID_hash_size);
        }
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"BalanceThread Terminated %d", 0);
    // return 0;
}

void CTestServer::BalanceUpdate()
{
    CMessage *msg;
    ull l_sessionID;

    WORD wType;

    CLockFreeQueue<CMessage*> *CotentsQ = &m_BalanceQ;

    
    // msg  크기 메세지 하나에 8Byte
    while (CotentsQ->m_size != 0)
    {

        if(CotentsQ->Pop(msg) == false)
            __debugbreak();

        l_sessionID = msg->ownerID;

        try
        {
            *msg >> wType;
        }
        catch (const MessageException &e)
        {
            switch (e.type())
            {
            case MessageException::ErrorType::HasNotData:
            {
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s %20s %05d  ",
                                                   L" msg >> Data  Faild ",
                                                   L"wType", wType);
                }
                break;
            }
            case MessageException::ErrorType::NotEnoughSpace:
            {
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s %20s %05d  ",
                                                   L"NotEnoughSpace  : ",
                                                   L"wType", wType);
                }
                msg->HexLog(CMessage::en_Tag::_ERROR, L"Attack.txt");
                break;
            }
            }
            stTlsObjectPool<CMessage>::Release(msg);
            Disconnect(l_sessionID);
            continue;
        }
        // prePlayer의 증가, 삭제를 담당.
        // Player_hash의 증가 , 삭제를 담당.
        switch (wType)
        {
        // 현재 미 사용중
        case en_PACKET_Player_Alloc:
            
            AllocPlayer(msg); // prePlayer의 증가.
            break;
        // TODO : 끊김과 직전의 메세지가 처리가 되지않을 수 있음.  순서가 달라져서 
        // 이렇게 하면 PlayerHash와 prePlayerHash는 ContentsThread에서는 접근할 일이 없음. 

        case en_PACKET_Player_Delete:
        {
            std::lock_guard<SharedMutex> SessionID_Hashlock(srw_SessionID_Hash);
            DeletePlayer(msg);
            break;
        }
        case en_PACKET_CS_CHAT_REQ_LOGIN:
        {
            std::lock_guard<SharedMutex> SessionID_Hashlock(srw_SessionID_Hash);
            if (PacketProc(l_sessionID, msg, wType) == false) // Login만 실행.
            {
                stTlsObjectPool<CMessage>::Release(msg);
                Disconnect(l_sessionID);
            }
            break;
        }
        }

    }

    {
        Profiler profile(L"HeartBeat");
        //HeartBeat();
    }

}

void CTestServer::HeartBeat()
{
    DWORD currentTime, playerTime;
    DWORD msgInterval;

    currentTime = timeGetTime();
    //// LoginPacket을 대기하는 하트비트 부분
    {

        stPlayer *player;

        // prePlayer_hash 는 LoginPacket을 대기하는 Session
        for (auto &element : prePlayer_hash)
        {
            player = element.second;

            playerTime = player->m_Timer;

            msgInterval = currentTime - playerTime;
            if (msgInterval >= 6000 && playerTime < currentTime)
            {
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %05lld %12s %05llu",
                                               L"TimeOver_prePlayer : ", player->m_AccountNo,
                                               L"현재들어온ID:", player->m_sessionID);
                Disconnect(player->m_sessionID);
            }
        }
    }

    //// LoginPacket을 받아서 승격된 하트비트 부분
    {
        stPlayer *player;

        // AccountNo_hash 는 LoginPacket을 받아서 승격된 Player
        for (auto &element : AccountNo_hash)
        {
            player = element.second;

            playerTime = player->m_Timer;
            msgInterval = currentTime - playerTime;

            if (msgInterval >= 40000 && playerTime < currentTime)
            {
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %05lld %12s %05llu",
                                               L"TimeOver_Player : ", player->m_AccountNo,
                                               L"현재들어온ID:", player->m_sessionID);
                Disconnect(player->m_sessionID);
            }
        }
    }
}

BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;
    HRESULT hr;
    
    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);

    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    RT_ASSERT(hMonitorThread != nullptr);
    hr = SetThreadDescription(hMonitorThread, L"\tMonitorThread");
    RT_ASSERT(!FAILED(hr));

    return retval;
}



void CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
{
    // double CurrentQ;
    ringBufferSize ContentsUseSize;
    CMessage **ppMsg;
    CLockFreeQueue<CMessage*> *TargetQ; //  Q 가 많아졌으므로 Q를 찾아오기.
    HANDLE hMsgQueuedEvent; // Q에 데이터가 들어왔음을 알림.

    std::wstring ProfileName; 

    if (bBalanceQ)
    {
        TargetQ = &m_BalanceQ; // Q랑 매핑되는 SRWLock을 획득하기.
        //pSrw = &srw_BalanceQ;
        hMsgQueuedEvent = hBalanceEvent;
        ProfileName = L"EnQueue_BalanceQ";
    }
    else
    {
        std::shared_lock<SharedMutex> SessionID_Hashlock(srw_SessionID_Hash);
        auto iter = SessionID_hash.find(SessionID);
        if (iter == SessionID_hash.end())
        {
            TargetQ = &m_BalanceQ; // Q랑 매핑되는 SRWLock을 획득하기.
            hMsgQueuedEvent = hBalanceEvent;
            ProfileName = L"EnQueue_BalanceQ";
        }
        else
        {
            stPlayer *player = iter->second;

            TargetQ = &m_CotentsQ_vec[player->m_ContentsQIdx];
            hMsgQueuedEvent = m_ContentsQMap[TargetQ];
            ProfileName = L"EnQueue_CotentsQ" + std::to_wstring(player->m_ContentsQIdx);
               
        }
    }
    ppMsg = &msg;
    {
        Profiler profile(L"Target_Enqeue");
        TargetQ->Push(msg);
    }
    SetEvent(hMsgQueuedEvent);
        
    _interlockedincrement64(&m_RecvTPS);
    ContentsUseSize = TargetQ->m_size;
    
}

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{
    // Accept의 요청이 밀리고 있다고한다면, 그 이후에 Accept로 들어오는 Session을 끊겠다.
    // m_AllocMsgCount 처리량을 보여주는 변수.
    LONG64 localAllocCnt;
    clsSession &session = sessions_vec[SessionID >> 47];

    localAllocCnt = _InterlockedIncrement64(&m_AllocMsgCount); // Alloc Message가 너무 쏟아진다면 인큐를 안함.
    if (localAllocCnt > m_AllocLimitCnt)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %4llu %10s %05lld  %10s %012llu  %10s %4llu  ",
                                       L"AllocMsg_Refuse",
                                       L"LocalMsgCnt", localAllocCnt,
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx);

        // 다시 감소 시키고, Session의 삭제 절차.
        localAllocCnt = _InterlockedDecrement64(&m_AllocMsgCount);
        // false를 반환하여 WSARecv를 걸지않고, Session에 대한 제거를 유도루틴을 타자.
        return false;
    }



    {
        char ipAddress[16];
        USHORT port;
        {
            sockaddr_in *ipv4 = (sockaddr_in *)&addr;
            inet_ntop(AF_INET, &ipv4->sin_addr, ipAddress, sizeof(ipAddress));
            port = ntohs(ipv4->sin_port);
        }

        CMessage *msg;

        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

        *msg << (unsigned short)en_PACKET_Player_Alloc;
        msg->PutData(ipAddress, 16);
        *msg << port; 
        InterlockedExchange(&msg->ownerID, SessionID);
        // TODO : 실패의 경우의 수
        OnRecv(SessionID, msg,true);

        //Win32::AtomicIncrement<LONG64>(m_NetworkMsgCount);
        _InterlockedIncrement64(&m_NetworkMsgCount);
    }

    return true;
}

void CTestServer::OnRelease(ull SessionID)
{
    {
        CMessage *msg;

        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
        *msg << (unsigned short)en_PACKET_Player_Delete;
        InterlockedExchange(&msg->ownerID, SessionID);

        OnRecv(SessionID, msg,true);

        _InterlockedIncrement64(&m_NetworkMsgCount);
    }
}

void MsgTypePrint(WORD type, LONG64 val)
{
    // 0 은 ㄴㄴ
    static const char *Msgformat[en_PACKET_CS_CHAT__Max] = {
        "",
        "en_PACKET_CS_CHAT_REQ_LOGIN",
        "en_PACKET_CS_CHAT_RES_LOGIN",
        "en_PACKET_CS_CHAT_REQ_SECTOR_MOVE",
        "en_PACKET_CS_CHAT_RES_SECTOR_MOVE",
        "en_PACKET_CS_CHAT_REQ_MESSAGE",
        "en_PACKET_CS_CHAT_RES_MESSAGE",
        "en_PACKET_CS_CHAT__HEARTBEAT",

    };
    printf("%20s : %05llu\n", Msgformat[type], val);
}
