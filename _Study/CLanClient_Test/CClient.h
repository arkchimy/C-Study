#pragma once
#include "CLanClient.h"


#include <unordered_map>
#include <shared_mutex>
#include <stack>

struct stPlayer
{
    ull _SessionID;
};
class CClient :public CLanClient
{
    bool PackProc(CMessage *msg);

    CClient(int PlayerMaxCnt);
  public:
	virtual void OnEnterJoinServer(ull SessionID); //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer(ull SessionID) ;     //< 서버와의 연결이 끊어졌을 때

    virtual void OnRecv(CMessage * msg) ;   //< 하나의 패킷 수신 완료 후
    virtual void OnSend(int sendsize) ; //	< 패킷 송신 완료 후
    virtual void OnRelease(ull SessionID);


    private:
        
        std::stack<stPlayer *> playerStack;
        std::unordered_map<ull, stPlayer *> SessionID_map;
        std::shared_mutex m_SessionMap;
};
