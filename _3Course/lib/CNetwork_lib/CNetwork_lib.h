#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <map>

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

class st_WSAData
{
  public:
    st_WSAData();
    ~st_WSAData();
};
using ull = unsigned long long;
class CLanServer
{
  public:

    ~CLanServer();
    // 오픈 IP / 포트 / 워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    BOOL Start(const wchar_t *bindAddress, short port, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    void Stop();
    int GetSessionCount();
    bool Disconnect(class clsSession *const session);

    void RecvComplete(class clsSession *const session, DWORD transferred);
    void SendComplete(class clsSession *const session, DWORD transferred);
    void PostComplete(class clsSession* const session, DWORD transferred);
    void SendPacket(class clsSession *const session);
    void RecvPacket(class clsSession *const session);


    /* 
            virtual bool OnConnectionRequest(IP, Port) = 0; //
        < accept 직후

        return false; 시 클라이언트 거부.
        return true;  시 접속 허용
 
            virtual void OnAccept(Client 정보 / SessionID / 기타등등) = 0;
        < Accept 후 접속처리 완료 후 호출.

            virtual void OnRelease(SessionID) = 0;
        < Release 후 호출
            

            virtual void OnRecv(SessionID, CPacket *) = 0;
        < 패킷 수신 완료 후

        //	virtual void OnSend(SessionID, int sendsize) = 0;           < 패킷 송신 완료 후

        //	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
        //	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후

        virtual void OnError(int errorcode, wchar *) = 0;
        
        */
    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();

  public:
    SOCKET m_listen_sock;
    HANDLE m_hIOCP;
    HANDLE *m_hThread;
    HANDLE m_hAccept;

    std::map<ull, class clsSession *> sessions;
};
