#include "LoginServer.h"

// Chatting Server와의 연결
// Redis와의 연결
// Client와의 연결

// OnRecv를 통해 Player객체에 접근 Player에 Overapped 을 두고 
// 

void MakeAndSendPacket()
{

}

void DBworkerThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    while (server->bOn)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(server->m_hDBIOCP, &transferred, &key, &overlapped, INFINITE);
        
    }
    
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

CTestServer::CTestServer(DWORD ContentsThreadCnt, int iEncording, int reduceThreadCount)
    : CLanServer(iEncording), m_ContentsThreadCnt(ContentsThreadCnt)
{
    DWORD lProcessCnt;
    if (GetLogicalProcess(lProcessCnt) == false)
        __debugbreak();

    // DBConcurrent Thread 세팅
    m_hDBIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

    HRESULT hr;
    player_pool.Initalize(m_maxPlayers);
    player_pool.Limite_Lock(); // Pool이 더 이상 늘어나지않음.

    // BalanceThread를 위한 정보 초기화.
    {
        m_ServerOffEvent = CreateEvent(nullptr, true, false, nullptr);
        m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    }
    // ContentsThread의 생성
    hContentsThread_vec.resize(ContentsThreadCnt);

    for (DWORD i = 0; i < ContentsThreadCnt; i++)
    {
        std::wstring ContentsThreadName = L"\tContentsThread" + std::to_wstring(i);

        hContentsThread_vec[i] = std::move(std::thread(DBworkerThread, this));

        RT_ASSERT(hContentsThread_vec[i].native_handle() != nullptr);

        hr = SetThreadDescription(hContentsThread_vec[i].native_handle(), ContentsThreadName.c_str());
        RT_ASSERT(!FAILED(hr));
    }

}

CTestServer::~CTestServer()
{

    for (DWORD i = 0; i < m_ContentsThreadCnt; i++)
    {
        hContentsThread_vec[i].join();
    }
}

float CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
{
    // Connect만하고 아무것도 안하는 얘가 존재할것임.
    // Session이 아닌 Player의 존재가 나와야하고, Player의 생성시기는? 
    // Accept를 받으면 바로 ContentsQ에 넣어서  생성.
    // 어차피 Update가 나와야함,  HeartBeat를 위해서 
    // 그 쓰레드에 넣고 Update를 돌림,
    // OnRecv를 받으면 PQCS를 통해서 DBIOCP에 일감을 넣어줄때 Player 정보에 들어있는 Overapped을 사용,
    // 
    std::shared_lock sessionHashLock(SessionID_hash_Lock);

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        return 0;

    CPlayer* player = SessionID_hash[SessionID];
    ZeroMemory(&player->DB_ReqOverlapped, sizeof(OVERLAPPED));

    player->DB_ReqOverlapped.msg = msg;
    player->DB_ReqOverlapped.SessionID = SessionID;

    PostQueuedCompletionStatus(m_hDBIOCP, 0, 0, (LPOVERLAPPED)&player->DB_ReqOverlapped);
    _interlockeddecrement64(&m_DBMessageCnt);

    return 0.0f;
}

void CTestServer::REQ_LOGIN(ull SessionID, CMessage* msg, INT64 AccountNo, WCHAR* SessionKey, WORD wType , BYTE bBroadCast , std::vector<ull>* pIDVector , WORD wVectorLen )
{

}