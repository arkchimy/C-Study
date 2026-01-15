// CZoneNetworkLib.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//
#include "CZoneNetworkLib.h"
// TODO: 라이브러리 함수의 예제입니다.
void fnCZoneNetworkLib()
{
    
    
        // IZone을 상속받아서 기능을 구현해둔다.
        class clsLoginZone : public IZone
        {
          public:
            // 이 함수를 컨텐츠 개발자가 구현해야함.
            virtual void OnEnterWorld(ull SessionId, SOCKADDR_IN &addr){};
            virtual void OnRecv(ull SessionId, struct CMessage *msg) {};
            virtual void OnUpdate() {};
            virtual void OnLeaveWorld(ull SessiondId){};
        };

        class clsContentsZone : public IZone
        {
          public:
            // 이 함수를 컨텐츠 개발자가 구현해야함.
            virtual void OnEnterWorld(ull SessionId, SOCKADDR_IN &addr){};
            virtual void OnRecv(ull SessionId, struct CMessage *msg) {};
            virtual void OnUpdate() {};
            virtual void OnLeaveWorld(ull SessiondId){};
        };
    

    // CZoneServer을 상속받는 Server 구현.
    class TestServer : public CZoneServer
    {
      public:
        TestServer(int iEncording, IZone *LoginZone)
            : CZoneServer(iEncording, LoginZone)
        {
            //LoginZone은 하나만 등록해야하고.
 
            clsContentsZone *zone = new clsContentsZone();
            // 동일한 방식으로 상속받아 구현한 class를 여기에 등록한다.
            RegisterZone(zone, L"Contents이름",20);
        }
    };
    //  TestServer생성시에 LoginZone을 넘겨주게 설계.
    clsLoginZone *Loginzone = new clsLoginZone();
    TestServer *server = new TestServer(1, Loginzone);


}

//  IOCP 에서 알려주는 용도
bool CZoneServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{

    clsSession& session = sessions_vec[SessionID >> 47];
    CMessage* msg = (CMessage*)stTlsObjectPool<CMessage>::Alloc();

    session._addr = addr;

    msg->ownerID = SessionID;

    session.m_zoneSet = m_LoginZone;
    m_LoginZone->Push(msg);


    return false;
}

//  IOCP 에서 알려주는 용도
void CZoneServer::OnRecv(ull SessionID, CMessage *msg)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    session.m_ZoneBuffer.Push(msg);
}

//  IOCP 에서 알려주는 용도
void CZoneServer::OnRelease(ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];

    _interlockedbittestandreset64((LONG64*)&session.m_ReleaseAndDBReQuest, 63);

}

void CZoneServer::RegisterLoginZone(IZone *zone, int deltaTime)
{
    // 이벤트 방식과 Frame 방식 을 고려해야할듯.
    // ContentsLogic의 경우는 프레임이 존재하므로 겸사겸사 하트비트가능.
    // LoginLogic의 경우 프레임으로 돌 이유가 없음. 이벤트가 필요.

    if(InterlockedCompareExchange(&_bLoginZoneChk,1,0) != 0)
    {
        // LoginZone 2개 이상 등록한 경우
        __debugbreak();
    }

    _hLoginEvent = CreateEvent(nullptr, 0, 0, nullptr);
    m_LoginZone = new ZoneSet(zone, L"LoginZoneThread", &bOn, deltaTime, _hLoginEvent);
    m_LoginZone->_server = this;
}

void CZoneServer::RegisterZone(IZone *zone, const wchar_t *ThreadName, int deltaTime)
{
    if (_bLoginZoneChk == false)
    {
        // LoginZone 등록되지 않은 경우
        __debugbreak();
    }

    ZoneSet *zoneSet = new ZoneSet(zone, ThreadName, &bOn, deltaTime);
    // 해당 Server가 관리하는 zoneSet목록
    _zoneList.push_back(zoneSet);
    zoneSet->_server = this;

}

void CZoneServer::RequeseMoveZone(ull SessionID, IZone *targetZone)
{
    //  이 단계에서 session이 Release될수 있을까?
    // RequeseMoveZone 의 호출은 속해있는 Zone 내부에서 호출한 것.
    // 동기적으로 이루어지므로 해제될 가능성 Zero

    clsSession &session = sessions_vec[SessionID >> 47];
    auto iter = _zoneMap.find(targetZone);
    if (iter == _zoneMap.end())
    {
        //_zoneMap 에 등록이 되지않은 상황.
        __debugbreak();
    }
    session.m_zoneSet = iter->second;
}
