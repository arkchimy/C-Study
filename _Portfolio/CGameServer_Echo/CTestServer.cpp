#include "CTestServer.h"
#include <Pdh.h>
#include <stdio.h>
#include "CZoneNetworkLib/CNetworkLib/utility/CCpuUsage/CCpuUsage.h"


#pragma comment(lib, "Pdh.lib")

void CTestServer::MonitorThread()
{
    timeBeginPeriod(1);

    DWORD currentTime;
    DWORD nextTime; // 내가 목표로하는 이상적인 시간.

    size_t MaxSessions = sessions_vec.size();

    char ProfilerFormat[2][30] = {
        "Profiler_Mode : Off\n",
        "Profiler_Mode : On\n"};

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

        currentTime = timeGetTime();
        nextTime; // 내가 목표로하는 이상적인 시간.
        nextTime = currentTime;
        LONG64 TotalTPS = 0;

        while (1)
        {
            nextTime += 1000;

            printf(" %-25s : %10lld\n", "PacketPool", stTlsObjectPool<CMessage>::instance.m_TotalCount);
            currentTime = timeGetTime();
            if (nextTime > currentTime)
                Sleep(nextTime - currentTime);
        }
        /*
        while (bMonitorThreadOn)
        {
            nextTime += 1000;
            {
                LONG64 old_UpdateTPS = m_UpdateTPS;
                UpdateTPS = old_UpdateTPS - before_UpdateTPS;
                before_UpdateTPS = old_UpdateTPS;
            }
            for (int i = 0; i <= m_WorkThreadCnt; i++)
            {
                LONG64 old_arrTPS = arrTPS[i];
                Send_arrTPS[i] = old_arrTPS - before_arrTPS[i];
                before_arrTPS[i] = old_arrTPS;
            }

            printf(" ============================================ Config ============================================ \n");

            printf("%-25s : %10d  %-25s : %10lld\n", "WorkerThread Cnt", m_WorkThreadCnt, "SessionNum", GetSessionCount());

            printf("%-25s : %10d  %-25s : %10d \n", "ZeroCopy", bZeroCopy, "Nodelay", bNoDelay);
            printf("%-25s : %10zu  %-25s : %10d\n", "MaxSessions", MaxSessions, "MaxPlayers", m_maxPlayers);

            printf(" \n========================================= Server Runtime Status ====================================== \n");

            printf(" %-25s : %10llu  \n", "Total Accept", getTotalAccept());
            printf(" %-25s : %10lld  \n", "MyCustomMessage_Cnt", getNetworkMsgCount());
            printf(" %-25s : %10lld  %-25s : %10lld\n", "PrePlayer Count", GetprePlayer_hash(),
                   "Player Count", GetPlayerCount());

            printf(" %-25s : %10lld\n", "PacketPool", stTlsObjectPool<CMessage>::instance.m_TotalCount);
            printf(" %-25s : %10lld\n", "Total iDisconnectCount", iDisCounnectCount);
            printf(" %100s \n", ProfilerFormat[Profiler::bOn]);

            printf(" ============================================ Contents Thread TPS ========================================== \n");

            printf(" Accept TPS           : %lld\n", Send_arrTPS[0]);
            printf(" Update TPS           : %lld\n", UpdateTPS);

            {
                LONG64 old_RecvTPS = m_RecvTPS;
                RecvTPS = old_RecvTPS - before_RecvTPS;
                before_RecvTPS = old_RecvTPS;
                printf(" Recv TPS : %lld\n", RecvTPS);
            }
            {
                TotalTPS = 0;
                for (int i = 1; i <= m_WorkThreadCnt; i++)
                {
                    TotalTPS += Send_arrTPS[i];
                    printf("%20s %10lld \n", "Send TPS :", Send_arrTPS[i]);
                }
                printf(" ===================================================================================================== \n");
                printf("%35s %10lld\n", "Total Send TPS :", TotalTPS);
                printf(" ===================================================================================================== \n");
            }

            {

                // Contents 정보 Print
                printf("%15s %10s %05d  %10s %05lld\n",
                       "Balance ",
                       "Player :", balanceVec[0],
                       "UpdateMessage_Queue", m_CotentsQ_vec[0].m_size);
                printf(" ===================================================================================================== \n");
                for (int idx = 1; idx < balanceVec.size(); idx++)
                {
                    printf("%15s %10s %05d  %10s %05lld\n",
                           "Contetent",
                           "Player :", balanceVec[idx],
                           "UpdateMessage_Queue", m_CotentsQ_vec[idx].m_size);
                }
            }
            printf(" ===================================================================================================== \n");

            // 메세지 별
            for (int i = 3; i < en_PACKET_CS_CHAT__Max - 1; i++)
            {
                LONG64 old_RecvMsg = m_RecvMsgArr[i];
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
                // PDH_FMT_COUNTERVALUE counterVal;

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

            time_t currenttt;
            time(&currenttt);
            // MonitorData
            {
                // InterlockedExchange((DWORD *)&g_MonitorData[enMonitorType::TimeStamp], currenttt);
                int totalCountentSize = 0;

                for (auto &contentQ : m_CotentsQ_vec)
                {
                    totalCountentSize += contentQ.m_size;
                }
                g_MonitorData[enMonitorType::TimeStamp] = (int)currenttt;
                g_MonitorData[enMonitorType::On] = bOn;
                g_MonitorData[enMonitorType::Cpu] = (int)CPUTime.ProcessTotal();
                g_MonitorData[enMonitorType::Memory] = (int)(Process_PrivateByteVal.largeValue / 1024 / 1024);
                g_MonitorData[enMonitorType::SessionCnt] = (int)GetSessionCount();
                g_MonitorData[enMonitorType::UserCnt] = (int)GetAccountNo_hash();
                g_MonitorData[enMonitorType::TotalSendTps] = (int)TotalTPS;
                g_MonitorData[enMonitorType::TotalPackPool_Cnt] = (int)stTlsObjectPool<CMessage>::instance.m_TotalCount;
                g_MonitorData[enMonitorType::UpdatePackPool_Cnt] = totalCountentSize;

                SetEvent(g_hMonitorEvent);
            }

            if (nextTime > currentTime)
                Sleep(nextTime - currentTime);
        }
        */
    }
}
