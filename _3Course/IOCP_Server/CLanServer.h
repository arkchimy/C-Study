#pragma once
using SessionID = unsigned long long;

class CLanServer
{
    bool Start(); // 오픈 IP / 포트 / 워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    void Stop();
    int GetSessionCount();

    bool Disconnect(SessionID);                  // SESSION_ID
    bool SendPacket(SessionID, class CPacket *); // SESSION_ID

    virtual bool OnConnectionRequest() = 0; //<< accept 직후
    //    return false;시 클라이언트 거부.
    //    return true;시 접속 허용

    virtual void OnAccept();                                          // OnClientJoin(Client 정보 / SessionID / 기타등등) = 0; //<< Accept 후 접속처리 완료 후 호출.
    virtual void OnRelease(SessionID sessionId) = 0;                  //    < Release 후 호출
    virtual void OnMessage(SessionID sessionId, class CPacket *) = 0; //<< 패킷 수신 완료 후

    //	virtual void OnSend(SessionID, int sendsize) = 0;           < 패킷 송신 완료 후

    //	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
    //	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후

    virtual void OnError(int errorcode, wchar_t *) = 0;

    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();
};
