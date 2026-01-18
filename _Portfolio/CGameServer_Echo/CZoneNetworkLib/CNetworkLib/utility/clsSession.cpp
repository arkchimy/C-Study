#include "clsSession.h"

#include "../utility/CSystemLog/CSystemLog.h"
#include "../utility/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../utility/CTlsObjectPool/CTlsObjectPool.h"

#include <timeapi.h>

#include "../CNetworkLib.h"
#include "../../CZoneNetworkLib.h"


ZoneSet::ZoneSet(IZone *zone, const wchar_t *ThreadName, int deltaTime, CZoneServer *server, HANDLE hEvent)

    : m_zone(zone), _deltaTime(deltaTime), _hEvent(hEvent), _bOn(true), _server(server)
{
    m_zone->_server = _server;
    if (_hEvent == INVALID_HANDLE_VALUE)
        m_Thread = WinThread(&ZoneSet::ZoneThread, this);
    else
        m_Thread = WinThread(&ZoneSet::ZoneTimerThread, this);

    SetThreadDescription(m_Thread.native_handle(), ThreadName);

}
void ZoneSet::ZoneThread()
{
    DWORD currentTime = timeGetTime();
    DWORD TargetTime = currentTime;
    CMessage *msg;

    timeBeginPeriod(1);
    while (_bOn == true)
    {

        TargetTime += _deltaTime;

        // Zone자체의 Q에서 빼기.
        while (q.Pop(msg))
        {
            ull SessionId = msg->ownerID;
            stTlsObjectPool<CMessage>::Release(msg);
            clsSession *session = _server->GetSession(SessionId);
            for (clsSession *element : sessions)
            {
                if (element == session)
                {
                    // 같은 Session 주소가 존재함.
                    __debugbreak();
                }
            }

            // session에 남아있는 msg 처분.
            while (session->m_ZoneBuffer.Pop(msg))
            {
                stTlsObjectPool<CMessage>::Release(msg);
            }
            sessions.push_back(session);
            m_zone->OnEnterWorld(SessionId, session->_addr);
         
        }
        for (clsSession *session : sessions)
        {
            while (session->m_ZoneBuffer.Pop(msg))
            {
                m_zone->OnRecv(session->m_SeqID, msg);
            }
        }
        m_zone->OnUpdate();
        for (auto iter = sessions.begin(); iter != sessions.end();)
        {
            clsSession *session = *iter;
            // DB요청도  IOCP에서도 연결끊김이 확인되면 호출
            if (session->m_ReleaseAndDBReQuest == 0)
            {
                ull SessionID = session->m_SeqID;
                ReleaseSession(*session);
                iter = sessions.erase(iter);

                m_zone->OnDisConnect(SessionID);
                continue;
            }
            else if (session->m_zoneSet != this)
            {
                // MoveZone이 호출됬다면 this 와 다름.
                if (session->m_ReleaseAndDBReQuest == (ull)1 << 63)
                {
                    // DB 처리가 끝났다면
                    ull SessionID;
                    ZoneSet *targetZone;
                    CMessage *msg;

                    SessionID = session->m_SeqID;
                    targetZone = session->m_zoneSet;

                    iter = sessions.erase(iter);
                    m_zone->OnLeaveWorld(SessionID);

                    msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

                    msg->ownerID = SessionID;
                    targetZone->Push(msg);

                    continue;
                }
            }

            iter++;
        }

        currentTime = timeGetTime();

        if (currentTime < TargetTime)
        {
            Sleep(TargetTime - currentTime);
        }
    }
    timeEndPeriod(1);
}


void ZoneSet::ZoneTimerThread()
{

    CMessage *msg;

    timeBeginPeriod(1);

    while (_bOn == true)
    {
        WaitForSingleObject(_hEvent, _deltaTime);
        // Zone자체의 Q에서 빼기.
        while (q.Pop(msg))
        {
            ull SessionId = msg->ownerID;
            stTlsObjectPool<CMessage>::Release(msg);

            clsSession* session = _server->GetSession(SessionId);
            for (clsSession* element : sessions)
            {
                if (element == session)
                {
                    //같은 Session 주소가 존재함.
                    __debugbreak();
                }
            }
            // 폐기
            while (session->m_ZoneBuffer.Pop(msg))
            {
                stTlsObjectPool<CMessage>::Release(msg);
            }
            sessions.push_back(session);
            m_zone->OnEnterWorld(SessionId, session->_addr);
     

        }
        for (clsSession *session : sessions)
        {
            while (session->m_ZoneBuffer.Pop(msg))
            {
                m_zone->OnRecv(session->m_SeqID, msg);
            }
        }
        m_zone->OnUpdate();
        for (auto iter = sessions.begin(); iter != sessions.end();)
        {
            clsSession *session = *iter;
            // DB요청도  IOCP에서도 연결끊김이 확인되면 호출
            if (session->m_ReleaseAndDBReQuest == 0)
            {
                ull SessionID = session->m_SeqID;
                ReleaseSession(*session);
                iter = sessions.erase(iter);

                m_zone->OnDisConnect(SessionID); 
                continue;
            }
            else if (session->m_zoneSet != this)
            {
                // MoveZone이 호출됬다면 this 와 다름.
                if (session->m_ReleaseAndDBReQuest == (ull)1 << 63)
                {
                    //DB 처리가 끝났다면
                    ull SessionID;
                    ZoneSet *targetZone;
                    CMessage *msg;


                    SessionID = session->m_SeqID;
                    targetZone = session->m_zoneSet;

                    iter = sessions.erase(iter);
                    m_zone->OnLeaveWorld(SessionID); 

                    msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

                    msg->ownerID = SessionID;
                    targetZone->Push(msg);

                    continue;
                    
                }

            }

            iter++;
        }
    }
    
  
    timeEndPeriod(1);
}


void ZoneSet::ReleaseSession(clsSession &session)
{
    session.Release();
    closesocket(session.m_sock);
    _server->PushSessionStack(session.m_SeqID);
}
