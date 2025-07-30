#pragma once
using SessionID = unsigned long long;

class CLanServer
{
    bool Start(); // ���� IP / ��Ʈ / ��Ŀ������ �� (������, ���׼�) / ���ۿɼ� / �ִ������� ��
    void Stop();
    int GetSessionCount();

    bool Disconnect(SessionID);                  // SESSION_ID
    bool SendPacket(SessionID, class CPacket *); // SESSION_ID

    virtual bool OnConnectionRequest() = 0; //<< accept ����
    //    return false;�� Ŭ���̾�Ʈ �ź�.
    //    return true;�� ���� ���

    virtual void OnAccept();                                          // OnClientJoin(Client ���� / SessionID / ��Ÿ���) = 0; //<< Accept �� ����ó�� �Ϸ� �� ȣ��.
    virtual void OnRelease(SessionID sessionId) = 0;                  //    < Release �� ȣ��
    virtual void OnMessage(SessionID sessionId, class CPacket *) = 0; //<< ��Ŷ ���� �Ϸ� ��

    //	virtual void OnSend(SessionID, int sendsize) = 0;           < ��Ŷ �۽� �Ϸ� ��

    //	virtual void OnWorkerThreadBegin() = 0;                    < ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
    //	virtual void OnWorkerThreadEnd() = 0;                      < ��Ŀ������ 1���� ���� ��

    virtual void OnError(int errorcode, wchar_t *) = 0;

    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();
};
