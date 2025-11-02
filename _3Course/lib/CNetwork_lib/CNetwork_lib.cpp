#include "CNetwork_lib.h"

#include "../Profiler_MultiThread/Profiler_MultiThread.h"
#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"

#include "stHeader.h"

#include <list>
#include <thread>

CSystemLog *CLanServer::systemLog = CSystemLog::GetInstance();
extern template PVOID stTlsObjectPool<CMessage>::Alloc();  // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지


BOOL DomainToIP(const wchar_t *szDomain, IN_ADDR *pAddr)
{
    ADDRINFOW *pAddrInfo;
    SOCKADDR_IN *pSockAddr;
    if (GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo) != 0)
    {
        return FALSE;
    }
    pSockAddr = (SOCKADDR_IN *)pAddrInfo->ai_addrlen;
    *pAddr = pSockAddr->sin_addr;
    FreeAddrInfo(pAddrInfo);
    return TRUE;
}
BOOL GetLogicalProcess(DWORD &out)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
    DWORD len;
    DWORD cnt;
    DWORD temp = 0;

    len = 0;

    GetLogicalProcessorInformation(infos, &len);

    infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];

    if (infos == nullptr)
        return false;

    GetLogicalProcessorInformation(infos, &len);

    cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문

    for (DWORD i = 0; i < cnt; i++)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            ULONG_PTR mask = infos[i].ProcessorMask;
            // 논리 프로세서의 수는 set된 비트의 개수
            while (mask)
            {
                temp += (mask & 1);
                mask >>= 1;
            }
        }
    }
    printf("LogicalProcess Cnt : %d \n", temp);

    delete[] infos;
    out = temp;
    return true;
}
SRWLOCK srw_Log;

unsigned AcceptThread(void *arg)
{

    /* arg 에 대해서
     * arg[0] 에는 클라이언트에게 연결시킬 hIOCP의 HANDLE을 넣기.
     * arg[1] 에는 listen_sock의 주소를 넣기
     */
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");
    SOCKET client_sock;
    SOCKADDR_IN addr;

    size_t *arrArg = (size_t *)arg;
    HANDLE hIOCP = (HANDLE)*arrArg;
    SOCKET listen_sock = SOCKET(arrArg[1]);

    DWORD listen_retval;

    DWORD flag;

    ull session_id = 0;
    ull idx;

    clsSession *session;
    CLanServer *server;

    int addrlen;
    stSessionId stsessionID;
    ull local_IoCount;
    // wchar_t buffer[100];

    addrlen = sizeof(addr);
    server = reinterpret_cast<CLanServer *>(arrArg[2]);

    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    flag = 0;

    InitializeSRWLock(&srw_Log);

    if (listen_retval == 0)
        printf("Listen Sucess\n");
    else
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Listen_Error");
    while (1)
    {
        client_sock = accept(listen_sock, (sockaddr *)&addr, &addrlen);
        if (client_sock == INVALID_SOCKET)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"Accept Error ");
            continue;
        }

        {

            if (server->m_IdxStack.m_size == 0)
            {
                closesocket(client_sock);
                // 추가적으로 구현해야되는 부분이 존재함.
                //  DisConnect 행위
                __debugbreak();
                continue;
            }
            server->m_IdxStack.Pop(idx);
        }

        session = &server->sessions_vec[idx];
        stsessionID.idx = idx;
        stsessionID.seqNumber = session_id++;
        
        
        InterlockedExchange(&session->m_sock, client_sock);
        InterlockedExchange(&session->m_blive, 1);
        InterlockedExchange(&session->m_flag, 0);
        InterlockedExchange(&session->m_ioCount, 1);            // 1로 시작하므로써 0으로 초기화때 Contents에서 오인하는 일을 방지.
        InterlockedExchange(&session->m_SeqID.SeqNumberAndIdx, stsessionID.SeqNumberAndIdx);
        InterlockedExchange(&session->m_Useflag, 0);
       

 /*       session->m_ioCount = 0;
        session->m_sock = client_sock;
        session->m_blive = 1;
        session->m_flag = 0;
        session->m_SeqID.SeqNumberAndIdx = (idx << 47 | session_id);
               InterlockedExchange(&session->m_Useflag, 0);
       */

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %d",
                                       L"Accept",
                                       L"UseFlag : ", 0);


        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu\n",
                                       L"Accept", L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);

        _interlockedincrement64(&server->m_SessionCount);
        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

        //InterlockedIncrement(&session->m_ioCount); // Login의 완료통지가 Recv전에 도착하면 IO_Count가 0이 되버리는 상황 방지.

        server->OnAccept(session->m_SeqID.SeqNumberAndIdx);
        server->RecvPacket(session);

        local_IoCount = InterlockedDecrement(&session->m_ioCount);// Accept시 1로 초기화 시킨 것 감소

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld %10s %05lld  %10s %012llu  %10s %4llu",
                                       L"Accept_Decrement",
                                       L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                       L"IO_Count", local_IoCount);

        if (local_IoCount == 0)
        {
            if (InterlockedExchange(&session->m_Useflag, 2) != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s %10s %d",
                                               L"AcceptThread",
                                               L"UseFlag : ", 2);
                continue;
            }

            if (InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0) != 0)
            {
                continue;
            }
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"AcceptThread",
                                           L"1<<47 : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", session->m_ioCount);

            ull seqID = session->m_SeqID.SeqNumberAndIdx;
            server->ReleaseSession(seqID);
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu ",
                                           L"AcceptRelease",
                                           L"HANDLE : ", session->m_sock, L"seqID :", seqID, L"seqIndx : ", seqID >> 47);
        }
    }
    return 0;
}

unsigned WorkerThread(void *arg)
{
    /*
     * arg[0] 에는 hIOCP의 가짜핸들을
     * arg[1] 에는 Server의 인스턴스
     */

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is WorkerThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is WorkerThread");
    size_t *arrArg = reinterpret_cast<size_t *>(arg);
    HANDLE hIOCP = (HANDLE)*arrArg;
    CLanServer *server = reinterpret_cast<CLanServer *>(arrArg[1]);

    static LONG64 s_arrTPS_idx = 0;
    LONG64 *arrTPS_idx = reinterpret_cast<LONG64 *>(TlsGetValue(server->m_tlsIdxForTPS));
    // arrTPS 의 index 값을 셋팅함.
    if (arrTPS_idx == nullptr)
    {
        TlsSetValue(server->m_tlsIdxForTPS, (void *)_interlockedincrement64(&s_arrTPS_idx));
    }

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session;

    ull local_IoCount;
    Profiler profile;


    while (server->bOn)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }

        GetQueuedCompletionStatus(hIOCP, &transferred, &key, &overlapped, INFINITE);

        if (overlapped == nullptr)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"GetQueuedCompletionStatus Overlapped is nullptr");
            continue;
        }
        session = reinterpret_cast<clsSession *>(key);
        // TODO : JumpTable 생성 되는 지?
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            //// FIN 의 경우에
            //if (transferred == 0)
            //    break;
            server->RecvComplete(session, transferred);
            break;
        case Job_Type::Send:
            server->SendComplete(session, transferred);
            break;

        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
            __debugbreak();
        }
        local_IoCount = InterlockedDecrement(&session->m_ioCount);
        
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"RecvComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
            break;
        case Job_Type::Send:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"SendComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
        }
        
        if (local_IoCount == 0)
        {
            //if (InterlockedExchange(&session->m_Useflag, 2) != 0)
            //{
            //    continue;
            //}

            /*if (InterlockedExchange(&session->m_Useflag, 2) != 0)
                continue;*/
            ull compareRetval = InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0);
            if (compareRetval != 0)
            {
              
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                                L"%-10s  %-10s  %llu %10s %05lld  %10s %018llu  %10s %4llu ",
                                               L"1 << 47Faild", L"retval : ", compareRetval,
                                               L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);
                
                continue;
            }
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"WorkerThread",
                                           L"1<<47 : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", session->m_ioCount);
            //TODO :  왜 순서를 다르게 했는지?
            // 새로 연결되어 0으로 초기화된 Session을 바꾸더라... 
            // 그래서 IoCount를 먼저 확인하고 flag를 바꾸는 식.


            // 순서를 다르게하면  이 부분에서 중단을 해놓고 기다린 후에 새로운 연결에 의해 0으로 초기화 될때 통과하면 
            // 연결끊기는거아님?
            // => 이 논리가 되려면 ContentsThread에서 끊을때 flag와 1<<47을 하나도 확인을 안해야함.
            compareRetval = InterlockedExchange(&session->m_Useflag, 2);
            if (compareRetval != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s  %-10s  %llu %10s %05lld  %10s %018llu  %10s %4llu ",
                                               L"m_Useflag Faild", L"retval : ", compareRetval,
                                               L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);
                continue;
            }
            ull seqID = session->m_SeqID.SeqNumberAndIdx;
            server->ReleaseSession(seqID);
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu ",
                                           L"WorkerRelease",
                                           L"HANDLE : ", session->m_sock, L"seqID :", seqID, L"seqIndx : ", seqID >> 47
                                           );
        }

    }
    printf("WorkerThreadID : %d  return '0' \n", GetCurrentThreadId());

    return 0;
}
CLanServer::CLanServer()
{

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_sock == INVALID_SOCKET)
    {

        ERROR_FILE_LOG(L"Socket_Error.txt",
                       L"listen_sock Create Socket Error");
        __debugbreak();
    }
}
CLanServer::~CLanServer()
{
    closesocket(m_listen_sock);
}

BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)
{
    linger linger;
    int buflen;
    DWORD lProcessCnt;
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;

    m_tlsIdxForTPS = TlsAlloc();
    if (m_tlsIdxForTPS == TLS_OUT_OF_INDEXES)
        __debugbreak();

    sessions_vec.resize(MaxSessions);

    for (ull idx = 0; idx < MaxSessions; idx++)
        m_IdxStack.Push(idx);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, bindAddress, &serverAddr.sin_addr);

    if (ZeroCopy)
    {
        bZeroCopy = true;
        buflen = 0;
        setsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
        getsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (char *)&buflen, &buflen);
        printf("getsockopt SendBuffer Len : %d \n", buflen);
    }

    setsockopt(m_listen_sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    setsockopt(m_listen_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

    bind_retval = bind(m_listen_sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bind_retval != 0)
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Bind Failed");

    if (GetLogicalProcess(lProcessCnt) == false)
        ERROR_FILE_LOG(L"GetLogicalProcessError.txt", L"GetLogicalProcess_Error");

    m_hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

    m_hThread = new HANDLE[WorkerCreateCnt];
    arrTPS = new LONG64[WorkerCreateCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (WorkerCreateCnt + 1));

    m_WorkThreadCnt = WorkerCreateCnt;

    {
        WorkerArg[0] = m_hIOCP;
        WorkerArg[1] = this;

        AcceptArg[0] = m_hIOCP;
        AcceptArg[1] = (HANDLE)m_listen_sock;
        AcceptArg[2] = this;
    }

    bOn = true;

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (int i = 0; i < WorkerCreateCnt; i++)
        m_hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &WorkerArg, 0, nullptr);

    return true;
}

void CLanServer::Stop()
{
}

bool CLanServer::Disconnect(const ull SessionID)
{
    //WorkerThread에서 호출하는 DisConnect이므로  IO가 '0' 이 되는 경우의 수가 없음. 
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK ) >> 47];
    ull local_IoCount;
    DWORD CancelRetval;

    if (InterlockedCompareExchange(&session.m_blive, 0,1) == 1)
    {
        CancelIO_Routine(SessionID);
        InterlockedIncrement(&iDisCounnectCount);
    }
    //Session의 ID는 변화가 없음을 의미.

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-10s %10s %05lld  %10s %018llu  %10s %4llu ",
                                   L"ContentsQueueFulled",
                                   L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx);

    //취소 요청을 찾을 수 없는 경우 반환 값은 0이고 GetLastError는 ERROR_NOT_FOUND를 반환
    //  함수가 성공하면 반환 값이 0이 아닙니다.

    return true;
}

bool CLanServer::DisconnectForContents(const ull SessionID)
{
    clsSession& session = sessions_vec[ SessionID  >> 47];
    ull SeqID;
    ull CompareRetval;

    //Release 가 2로 바꾸었다는 뜻.
    //true면 메세지를 폐기.
    //false면 끊기 판정 아님
    CompareRetval = InterlockedCompareExchange(&session.m_Useflag, 1, 0);
    if (CompareRetval != 0)
    {
        return true;
    }
    if (session.m_SeqID.SeqNumberAndIdx != SessionID)
    {
        // 다른 세션이었지만 이미 Flag를 물었고, 물고나보니 Release가 시도 됬을때
        if (InterlockedCompareExchange(&session.m_Useflag, 0,1) == 2)
        {
            
            SeqID = session.m_SeqID.SeqNumberAndIdx;
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"DisConnectForContents1",
                                           L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                           L"IO_Count", session.m_ioCount);
            ReleaseSession(SeqID);
        }
        return true;
    }
    // m_blive는 
    if (InterlockedCompareExchange(&session.m_blive, 0,1) == 1)
    {
        InterlockedIncrement(&iDisCounnectCount);
    }
    CancelIO_Routine(SessionID);
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %-10s %4llu %-10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"DisconnectForContents", L"m_Useflag : ", CompareRetval, L"Handle",
                                   session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                   L"IO_Count", session.m_ioCount);
    // 취소 요청을 찾을 수 없는 경우 반환 값은 0이고 GetLastError는 ERROR_NOT_FOUND를 반환
    

    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
    {
 
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"1 << 47 ForContents2",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        ReleaseSession(SessionID);
     
        return true;
    }
    else
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu ",
                                       L"ForContents2Failed",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx);
    }

    if (InterlockedCompareExchange(&session.m_Useflag, 0, 1) == 2)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu ",
                                       L"DisconnectForContents3",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx);
        ReleaseSession(SessionID);
    
        return true;
    }
    else
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu ",
                                       L"ForContents3Failed",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx);
    }

    return false;
}

void CLanServer::CancelIO_Routine(const ull SessionID)
{
    //Session에 대한 안정성은  외부에서 보장해주세요.
    BOOL retval;
    clsSession &session = sessions_vec[SessionID >> 47];

    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);

}

int CLanServer::GetSessionCount()
{
    // AcceptThread에서 전담한다면, interlock이 필요없다.
    return m_SessionCount;
}

LONG64 CLanServer::GetReleaseSessions()
{
    return m_ReleaseSessions.m_size;
}

LONG64 CLanServer::Get_IdxStack()
{
    return m_IdxStack.m_size;
}

void CLanServer::RecvComplete(clsSession *const session, DWORD transferred)
{
    session->m_recvBuffer.MoveRear(transferred);

    stHeader header;
    ringBufferSize useSize;
    float qPersentage;

    char *f, *r;
    f = session->m_recvBuffer._frontPtr;
    r = session->m_recvBuffer._rearPtr;

    // ContentsQ의 상황이 어떤지를 체크.
    qPersentage = OnRecv((ull)(session->m_SeqID.SeqNumberAndIdx), nullptr);
    if (qPersentage >= 75.f)
    {
        Disconnect(session->m_SeqID.SeqNumberAndIdx);
        return;
    }
    // Header의 크기만큼을 확인.
    while (session->m_recvBuffer.Peek(&header, sizeof(stHeader), f, r) == sizeof(stHeader))
    {
        useSize = session->m_recvBuffer.GetUseSize(f, r);
        if (useSize < header._len + sizeof(stHeader))
        {
            Disconnect(session->m_SeqID.SeqNumberAndIdx);
            return;
        }
        // 메세지 생성
        CMessage *msg = CreateMessage(session, header);
        {
            Profiler profile;
            profile.Start(L"OnRecv");
            qPersentage = OnRecv(session->m_SeqID.SeqNumberAndIdx, msg);
            profile.End(L"OnRecv");
        }
        f = session->m_recvBuffer._frontPtr;
        if (qPersentage >= 75.0)
        {
            Disconnect(session->m_SeqID.SeqNumberAndIdx);
            return;
        }
    }

    RecvPacket(session);
}

CMessage *CLanServer::CreateMessage(clsSession *const session, class stHeader &header)
{
    unsigned short type = 0;
    Profiler profile;

    profile.Start(L"PoolAlloc");
    //CMessage *msg = reinterpret_cast<CMessage *>(CMessagePoolManager::pool.Alloc());
    CMessage *msg = reinterpret_cast<CMessage *> (stTlsObjectPool<CMessage>::Alloc());
    profile.End(L"PoolAlloc");

  

    ringBufferSize deQsize;

    switch (type)
    {
    case 0:
    default:
    {
        deQsize = session->m_recvBuffer.Dequeue(msg->_begin, sizeof(header) + header._len);
        msg->_rearPtr = msg->_begin + deQsize;
        msg->_begin = msg->_begin;
    }
    }
    return msg;
}
char *CLanServer::CreateLoginMessage()
{
    static short header = 0x0008;
    static ull PayLoad = 0x7fffffffffffffff;
    static char msg[10];

    memcpy(msg, &header, sizeof(header));
    memcpy(msg + sizeof(header), &PayLoad, sizeof(PayLoad));

    return msg;
}


void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    CMessage *msg;

    while (session->m_SendMsg.Pop(msg))
    {
        stTlsObjectPool<CMessage>::Release(msg);
    }
    SendPacket(session);

}

void CLanServer::SendPacket(clsSession *const session)
{

    ringBufferSize useSize, directDQSize;
    ringBufferSize deQSize;

    DWORD bufCnt;
    int send_retval = 0;

    WSABUF wsaBuf[500];
    DWORD LastError;
    // LONG64 beforeTPS;
    LONG64 TPSidx;
    ull local_IoCount;

    useSize = session->m_sendBuffer.m_size;
    TPSidx = (LONG64)TlsGetValue(m_tlsIdxForTPS);

    if (useSize == 0)
    {
        // flag 끄기
        if (_InterlockedCompareExchange(&session->m_flag, 0, 1) == 1)
        {
            useSize = session->m_sendBuffer.m_size;
            if (useSize != 0)
            {
                // 누군가 진입 했다면 return
                if (_InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
                {
                    ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
                    InterlockedIncrement(&session->m_ioCount);
                    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_sendOverlapped);
                }
            }
        }
        return;
    }

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
    }

    ull msgAddr;
    CMessage *msg;

    bufCnt = 0;

    {
        Profiler profile;
        profile.Start(L"LFQ_Pop");
        while (session->m_sendBuffer.Pop(msg))
        {

            session->m_SendMsg.Push(msg);

            wsaBuf[bufCnt].buf = msg->_frontPtr;
            wsaBuf[bufCnt].len = SerializeBufferSize(msg->_rearPtr - msg->_frontPtr);
            bufCnt++;
            useSize -= 8;
        }
        profile.End(L"LFQ_Pop");
    }

    arrTPS[TPSidx] += bufCnt;

    if (session->m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session->m_ioCount);
 
        if (bZeroCopy)
        {
            Profiler::Start(L"ZeroCopy WSASend");
            send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
            LastError = GetLastError();
            Profiler::End(L"ZeroCopy WSASend");
        }
        else
        {
            Profiler::Start(L"WSASend");

            send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
            LastError = GetLastError();
            Profiler::End(L"WSASend");
        }

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSASend",
                                       L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        if (send_retval < 0)
            WSASendError(LastError, session->m_SeqID.SeqNumberAndIdx);
    }

}

void CLanServer::RecvPacket(clsSession *const session)
{
    ringBufferSize freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval = 0;
    DWORD LastError;
    DWORD flag = 0;
    DWORD bufCnt;
    ull local_IoCount;

    char *f = session->m_recvBuffer._frontPtr, *r = session->m_recvBuffer._rearPtr;

    {
        ZeroMemory(session->m_lastRecvWSABuf, sizeof(session->m_lastRecvWSABuf));
        ZeroMemory(&session->m_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session->m_recvBuffer.DirectEnqueueSize(f, r);
    freeSize = session->m_recvBuffer.GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.
    if (freeSize < directEnQsize)
        __debugbreak();
    if (freeSize <= directEnQsize)
    {
        session->m_lastRecvWSABuf[0].buf = r;
        session->m_lastRecvWSABuf[0].len = directEnQsize;

        bufCnt = 1;
    }
    else
    {
        session->m_lastRecvWSABuf[0].buf = r;
        session->m_lastRecvWSABuf[0].len = directEnQsize;

        session->m_lastRecvWSABuf[1].buf = session->m_recvBuffer._begin;
        session->m_lastRecvWSABuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    if (session->m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session->m_ioCount);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSARecv",
                                       L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        wsaRecv_retval = WSARecv(session->m_sock, session->m_lastRecvWSABuf, bufCnt, nullptr, &flag, &session->m_recvOverlapped, nullptr);

        LastError = GetLastError();
        if (wsaRecv_retval <0)
            WSARecvError(LastError, session->m_SeqID.SeqNumberAndIdx);
    }
    //else
    //{
    //    local_IoCount = _InterlockedDecrement(&session->m_ioCount);
    //    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
    //                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
    //                                   L"WSARecv_liveZero",
    //                                   L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
    //                                   L"IO_Count", local_IoCount);
    //}

}

int CLanServer::getAcceptTPS()
{
    return 0;
}
int CLanServer::getRecvMessageTPS()
{
    return 0;
}
int CLanServer::getSendMessageTPS()
{
    return 0;
}

void CLanServer::WSASendError(const DWORD LastError,const ull SessionID)
{
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull local_IoCount;

    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (InterlockedCompareExchange(&session.m_blive, 0, 0) == 0)
        {
            //CancelIO_Routine(SessionID);
            CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
        }
        break;

    case WSAEINTR://10004
    case 10038:
    case 10053:
    case 10054:
        InterlockedExchange(&session.m_blive, 0);
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:

 
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ],%10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Send_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        InterlockedExchange(&session.m_blive, 0);
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
   

}

void CLanServer::WSARecvError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull local_IoCount;
    DWORD CancelRetval;

    switch (LastError)
    {
        // 10004   CancelIO
    case WSA_IO_PENDING:
        if (InterlockedCompareExchange(&session.m_blive,0,0) == 0)
        {
            // 함수가 성공하면 반환 값이 0이 아닙니다.
            //CancelIO_Routine(SessionID);
            CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
        }
        break;

    case WSAEINTR: // 10004
    case 10038:
    case 10053:
    case 10054:
        InterlockedExchange(&session.m_blive, 0);
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ] %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Recv_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        InterlockedExchange(&session.m_blive, 0);
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }


}

void CLanServer::ReleaseSession(ull SessionID)
{
    clsSession* session = &sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    thread_local clsSession temp;
    
    temp = *session;
    if (session->m_SeqID.SeqNumberAndIdx != SessionID)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %012llu",
                                       L"SeqNum!=SessionID",
                                       L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                       L"SessionID", SessionID);
        return;
    }
    session->Release();
    closesocket(temp.m_sock);
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                    L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                    L"Closesocket",
                                   L"HANDLE : ", temp.m_sock, L"seqID :", temp.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", temp.m_SeqID.idx,
                                   L"IO_Count", temp.m_ioCount);
    m_IdxStack.Push(SessionID >> 47);
    _interlockeddecrement64(&m_SessionCount);
}
