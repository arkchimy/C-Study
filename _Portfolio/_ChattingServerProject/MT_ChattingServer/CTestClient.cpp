#include "CTestClient.h"
#include "MonitorData.h"


CTestClient::CTestClient(bool bEncoding)
    : CLanClient(bEncoding)
{
    _hTimer = WinThread(&CTestClient::TimerThread, this);

}

void CTestClient::TimerThread()
{
    CMessage *msg;

    while (1)
    {
        WaitForSingleObject(g_hMonitorEvent, INFINITE);

        for (int i = 1; i < enMonitorType::Max; i++)
        {
            msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

            *msg << en_PACKET_SS_MONITOR_DATA_UPDATE;
            *msg << (BYTE)(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN + i - 1);
            *msg << g_MonitorData[i];
            *msg << g_MonitorData[0];// timeStamp
            PostReQuest_iocp(msg);

        }

        //------------------------------------------------------------
        // 서버가 모니터링서버로 데이터 전송
        // 각 서버는 자신이 모니터링중인 수치를 1초마다 모니터링 서버로 전송.
        //
        // 서버의 다운 및 기타 이유로 모니터링 데이터가 전달되지 못할떄를 대비하여 TimeStamp 를 전달한다.
        // 이는 모니터링 클라이언트에서 계산,비교 사용한다.
        //
        //	{
        //		WORD	Type
        //
        //		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
        //		int		DataValue				// 해당 데이터 수치.
        //		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
        //										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
        //										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
        //	}
        //
        //------------------------------------------------------------
        // en_PACKET_SS_MONITOR_DATA_UPDATE,

    }
}


void CTestClient::OnEnterJoinServer()
{
    //pqcs를 통해 내  IOCP 에 REQ_MONITOR_LOGIN 메세지를 만들어 PQCS 한다.
    CMessage* msg =  (CMessage*)stTlsObjectPool<CMessage>::Alloc();
    // 내가 직접 넣는다고치고 PQCS를 하면 디코드를 하지않을까. 
    // 그러면 Decode를 하지않고, 바로 받아보자. 
    // 보낼때만 Encode하는 방식
    int ServerNo = 1;

    *msg << en_PACKET_SS_MONITOR_LOGIN;
    *msg << ServerNo;
    
    PostReQuest_iocp(msg);

}

void CTestClient::OnLeaveServer()
{

}

void CTestClient::OnRecv(CMessage *msg)
{
    WORD type;
    *msg >> type;
    if (PacketProc(0, msg, type) == false)
        Disconnect();
}

void CTestClient::REQ_MONITOR_LOGIN( CMessage *msg, int ServerNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    
    Proxy::RES_MONITOR_LOGIN(msg, ServerNo );
}

void CTestClient::REQ_MONITOR_UPDATE(CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    
    Proxy::RES_MONITOR_UPDATE(msg, DataType, DataValue, TimeStamp);
}
