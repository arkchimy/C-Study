#include "clsSession.h"

#include "../utility/CSystemLog/CSystemLog.h"
#include "../utility/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../utility/CTlsObjectPool/CTlsObjectPool.h"

#include <timeapi.h>



ZoneSet::ZoneSet(IZone *zone, const wchar_t *ThreadName, bool *bOn, int deltaTime, HANDLE hEvent)

    : m_zone(zone), m_bOn(bOn), _deltaTime(deltaTime), _hEvent(hEvent)
{

    m_Thread = WinThread(&ZoneSet::ThreadMain, this);
    SetThreadDescription(m_Thread.native_handle(), ThreadName);
}

void ZoneSet::ThreadMain()
{
    DWORD currentTime = timeGetTime();
    DWORD TargetTime = currentTime;
    CMessage *msg;

    timeBeginPeriod(1);

    if (_hEvent != INVALID_HANDLE_VALUE)
    {
        while (*m_bOn == true)
        {
            WaitForSingleObject(_hEvent, _deltaTime);
            // Zone자체의 Q에서 빼기.
            while (q.Pop(msg))
            {
                ull SessionId = msg->ownerID;
                m_zone->OnRecv(SessionId, msg);
            }
            for (clsSession *session : sessions)
            {
                while (session->m_ZoneBuffer.Pop(msg))
                {
                    m_zone->OnRecv(session->m_SeqID, msg);
                }
            }
            m_zone->OnUpdate();
            m_zone->OnRelease(0); // 연결이 끊어진 session에 대한 판정을 어떻게할지는 또 고민이네...

        }
    }
    else
    {
        while (*m_bOn == true)
        {

            TargetTime += _deltaTime;

            // Zone자체의 Q에서 빼기.
            while (q.Pop(msg))
            {
                ull SessionId = msg->ownerID;
                m_zone->OnRecv(SessionId, msg);
            }
            for (clsSession *session : sessions)
            {
                while (session->m_ZoneBuffer.Pop(msg))
                {
                    m_zone->OnRecv(session->m_SeqID, msg);
                }
            }
            m_zone->OnUpdate();
            m_zone->OnRelease(0); // 연결이 끊어진 session에 대한 판정을 어떻게할지는 또 고민이네...

            currentTime = timeGetTime();

            if (currentTime < TargetTime)
            {
                Sleep(TargetTime - currentTime);
            }
        }
    }
    timeEndPeriod(1);
}
