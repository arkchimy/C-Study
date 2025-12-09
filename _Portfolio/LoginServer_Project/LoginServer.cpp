#include "LoginServer.h"
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

// Chatting Server와의 연결
// Redis와의 연결
// Client와의 연결

// OnRecv를 통해 Player객체에 접근 Player에 Overapped 을 두고 
// 

enum en_PACKET_CS_LOGIN_RES_LOGIN : BYTE
{
    dfLOGIN_STATUS_NONE = -1,        // 미인증상태
    dfLOGIN_STATUS_FAIL = 0,         // 세션오류
    dfLOGIN_STATUS_OK = 1,           // 성공
    dfLOGIN_STATUS_GAME = 2,         // 게임중
    dfLOGIN_STATUS_ACCOUNT_MISS = 3, // account 테이블에 AccountNo 없음
    dfLOGIN_STATUS_SESSION_MISS = 4, // Session 테이블에 AccountNo 없음
    dfLOGIN_STATUS_STATUS_MISS = 5,  // Status 테이블에 AccountNo 없음
    dfLOGIN_STATUS_NOSERVER = 6,     // 서비스중인 서버가 없음.
};
BYTE CTestServer::WaitDB(INT64 AccountNo, const WCHAR *const SessionKey, WCHAR *ID, WCHAR *Nick)
{
    //일단은 성공만 반환 나중에 db에서 가져와서 확인.
    Sleep(10); // DB의 I/O요청을 고려해서 Sleep 을 둠.

    ZeroMemory(ID, 40);
    ZeroMemory(Nick, 40);

    return dfLOGIN_STATUS_OK;
}
WCHAR GameServerIP[16] = L"0.0.0.0";
USHORT GameServerPort = 0 ;
WCHAR ChatServerIP[16] = L"106.245.38.108";
USHORT ChatServerPort = 6000;

void CTestServer::DB_VerifySession(ull SessionID, CMessage *msg)
{
    INT64 AccountNo;
    WCHAR SessionKey[32];
    BYTE retval;
    WCHAR ID[20];      // 사용자 ID		. null 포함
    WCHAR Nickname[20]; // 사용자 닉네임	. null 포함

    *msg >> AccountNo;
    msg->GetData(SessionKey, 64);

    retval = WaitDB(AccountNo, SessionKey, ID,Nickname);
    RES_LOGIN(SessionID, msg, AccountNo, retval, ID, Nickname, GameServerIP, GameServerPort, ChatServerIP, ChatServerPort);

}

void DBworkerThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    stDBOverapped *dbOverlapped;

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
        _interlockeddecrement64(&server->m_DBMessageCnt);
        dbOverlapped = reinterpret_cast<stDBOverapped *>(overlapped);
        if (overlapped == nullptr)
            __debugbreak();
        // TODO : DB_VerifySession의 반환값에 따른 동작이 이게 맞나?
        server->DB_VerifySession(dbOverlapped->SessionID,dbOverlapped->msg);

    }
    
}


CTestServer::CTestServer(DWORD ContentsThreadCnt, int iEncording, int reduceThreadCount)
    : CLanServer(iEncording), m_ContentsThreadCnt(ContentsThreadCnt)
{

    // DBConcurrent Thread 세팅
    m_hDBIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, m_lProcessCnt - reduceThreadCount);

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
BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;
    HRESULT hr;

    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);

    //hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    //RT_ASSERT(hMonitorThread != nullptr);
    //hr = SetThreadDescription(hMonitorThread, L"\tMonitorThread");
    //RT_ASSERT(!FAILED(hr));

    return retval;
}

float CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
{
    WORD type;
    *msg >> type;
    PacketProc(SessionID,msg,type);
    return 0.0f;
}

bool CTestServer::OnAccept(ull SessionID)
{
    return false;
}

void CTestServer::OnRelease(ull SessionID)
{
}

void CTestServer::REQ_LOGIN(ull SessionID, CMessage* msg, INT64 AccountNo, WCHAR* SessionKey, WORD wType , BYTE bBroadCast , std::vector<ull>* pIDVector , WORD wVectorLen )
{
    //NetworkThread가 진입.
    std::shared_lock sessionHashLock(SessionID_hash_Lock);

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        return;

    CPlayer *player = SessionID_hash[SessionID];
    ZeroMemory(&player->DB_ReqOverlapped, sizeof(OVERLAPPED));

    *msg << AccountNo;
    msg->PutData(SessionKey, 64);
    player->DB_ReqOverlapped.msg = msg;
    player->DB_ReqOverlapped.SessionID = SessionID;

    PostQueuedCompletionStatus(m_hDBIOCP, 0, 0, (LPOVERLAPPED)&player->DB_ReqOverlapped);
    _interlockedincrement64(&m_DBMessageCnt);
}

void CTestServer::AllocPlayer(CMessage *msg)
{
}

void CTestServer::DeletePlayer(CMessage *msg)
{
}

void CTestServer::Update()
{
}
