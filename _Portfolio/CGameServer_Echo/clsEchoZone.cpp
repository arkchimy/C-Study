#include "clsEchoZone.h"
#include "clsLoginZone.h"

void clsEchoZone::OnEnterWorld(ull SessionID, SOCKADDR_IN &addr)
{
    clsLoginZone *loginZone;
    CMessage *msg;

    {
        //해당 SessionID 매칭 데이터가 있다면 말이 안 됨.
        auto iter = SessionID_hash.find(SessionID);
        if (iter != SessionID_hash.end())
            __debugbreak();

        {
            //해당  Session이 존재하는지 판단.
            std::shared_lock<SharedMutex> lock(_server->_zoneMutex);

            auto iter = _server->_zoneMap.find((ZoneKeyType)enZoneType::LoginZone);
            if (iter == _server->_zoneMap.end())
                __debugbreak();
            loginZone = dynamic_cast<clsLoginZone *>(iter->second->GetZone());
        }

        // TODO : 이게 최선인가.
        {
            std::shared_lock<SharedMutex> lock(loginZone->_SessionTable_Mutex);
            auto iter = loginZone->SessionID_hash.find(SessionID);
            if (iter == loginZone->SessionID_hash.end())
                __debugbreak();
            stPlayer *player = iter->second;

            SessionID_hash.insert({SessionID, player});

            msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
            RES_LOGIN(SessionID, msg, 1, player->_AccountNo);
        }

    }
}

void clsEchoZone::OnRecv(ull SessionID, CMessage *msg)
{
    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
    {
        stTlsObjectPool<CMessage>::Release(msg);
        _server->Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"OnDisConnect", en_LOG_LEVEL::ERROR_Mode, L"EchoZone_DisConnect %20s SessionID : %lld ",
                                       L"SessionID_hash Not Found", SessionID);
        return;
    }
    if (PacketProc(SessionID, msg) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        _server->Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"OnDisConnect", en_LOG_LEVEL::ERROR_Mode, L"EchoZone_DisConnect %20s SessionID : %lld ",
                                       L" PacketProc false return ", SessionID);
        return;
    }
    stPlayer *player = iter->second;
    player->_lastRecvTime = timeGetTime();
    totalPacketCnt++;
}

void clsEchoZone::OnUpdate()
{
    DWORD currentTime = timeGetTime();
    DWORD distance;

    /*for (auto& iter : SessionID_hash)
    {
        stPlayer *player = iter.second;
        distance = currentTime - player->_lastRecvTime;
        if (distance >= _sessionTimeoutMs)
        {
            _server->Disconnect(player->_SessionID);
        }
    }*/
}

void clsEchoZone::OnLeaveWorld(ull SessionID)
{
    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
        __debugbreak();
    SessionID_hash.erase(SessionID);

}

void clsEchoZone::OnDisConnect(ull SessionID)
{
    clsLoginZone *loginZone;
    stPlayer *player;

    //CSystemLog::GetInstance()->Log(L"OnDisConnect", en_LOG_LEVEL::ERROR_Mode, L"EchoZone_DisConnect %lld", SessionID);
    // 해당 SessionID 매칭 데이터가 없다면 말이 안 됨.
    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
        __debugbreak();
    
    {
        std::shared_lock<SharedMutex> lock(_server->_zoneMutex);

        auto iter = _server->_zoneMap.find((ZoneKeyType)enZoneType::LoginZone);
        if (iter == _server->_zoneMap.end())
            __debugbreak();
        loginZone = dynamic_cast<clsLoginZone *>(iter->second->GetZone());
    }

    // TODO : 이게 최선인가.

    {
        std::lock_guard<SharedMutex> lock(loginZone->_SessionTable_Mutex);
        auto iter = loginZone->SessionID_hash.find(SessionID);
        if (iter == loginZone->SessionID_hash.end())
            __debugbreak();
        player = iter->second;

        auto Accountiter = loginZone->Account_hash.find(player->_AccountNo);

        // 중복 로그인이라면 있던 player를 끊음.
        if (Accountiter != loginZone->Account_hash.end())
        {
            loginZone->Account_hash.erase(Accountiter);
        }

        SessionID_hash.erase(SessionID);
        loginZone->SessionID_hash.erase(iter);
    }
    // Player반환은 여기서.
    loginZone->player_pool.Release(player);

}

bool clsEchoZone::PacketProc(ull SessionID, CMessage *msg)
{
    WORD wType;
    try
    {
        *msg >> wType;
    }
    catch (const MessageException &e)
    {
        switch (e.type())
        {
        case MessageException::ErrorType::HasNotData:
        {
            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %20s %05d  ",
                                               L" msg >> Data  Faild ",
                                               L"wType", wType);
            }
            break;
        }
        case MessageException::ErrorType::NotEnoughSpace:
        {
            static bool bOn = false;
            if (bOn == false)
            {
                bOn = true;
                CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %20s %05d  ",
                                               L"NotEnoughSpace  : ",
                                               L"wType", wType);
            }
        }
            msg->HexLog(CMessage::en_Tag::_ERROR, L"Attack.txt");
            break;
        }

        return false;
    }
    switch (wType)
    {
    case en_PACKET_CS_GAME_REQ_ECHO:
    {
        try
        {
            INT64 AccountNo;
            LONGLONG SendTick;
            *msg >> AccountNo;
            *msg >> SendTick;
            REQ_ECHO(SessionID, msg, AccountNo, SendTick);
        }
        catch (const MessageException &e)
        {
            switch (e.type())
            {
            case MessageException::ErrorType::HasNotData:
                break;
            case MessageException::ErrorType::NotEnoughSpace:
                break;
            }
            return false;
        }
        _msgTypeCntArr[EchoPacket]++;
        break;
    } 
    case en_PACKET_CS_GAME_REQ_HEARTBEAT:
    {
        try
        {
            REQ_HEARTBEAT(SessionID, msg);
        }
        catch (const MessageException &e)
        {
            switch (e.type())
            {
            case MessageException::ErrorType::HasNotData:
                break;
            case MessageException::ErrorType::NotEnoughSpace:
                break;
            }
            return false;
        }
        _msgTypeCntArr[HeartBeatPacket]++;
        break;
    }
    default:
        return false;
    }

    return true;
}

void clsEchoZone::RES_LOGIN(ull SessionID, CMessage *msg, BYTE Status, INT64 AccountNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stHeader header;

    header.byCode = 0x77;
    msg->InitMessage();

    msg->PutData(&header, sizeof(stHeader));
    *msg << wType;
    *msg << Status;
    *msg << AccountNo;
    short *pheaderlen = (short *)(msg->_frontPtr + offsetof(stHeader, sDataLen));
    *pheaderlen = msg->_rearPtr - msg->_frontPtr - _server->GetheaderSize();

    if (_server->GetisEncode())
        msg->EnCoding();

    _server->SendPacket(SessionID, msg, bBroadCast, pIDVector, wVectorLen);
}

void clsEchoZone::REQ_ECHO(ull SessionID, CMessage *msg, INT64 AccountNo, LONGLONG SendTick, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    RES_ECHO(SessionID, msg, AccountNo, SendTick);
}

void clsEchoZone::RES_ECHO(ull SessionID, CMessage *msg, INT64 AccountNo, LONGLONG SendTick, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stHeader header;

    header.byCode = 0x77;
    msg->InitMessage();

    msg->PutData(&header, sizeof(stHeader));
    *msg << wType;
    *msg << AccountNo;
    *msg << SendTick;
    short *pheaderlen = (short *)(msg->_frontPtr + offsetof(stHeader, sDataLen));
    *pheaderlen = msg->_rearPtr - msg->_frontPtr - _server->GetheaderSize();
    if (_server->GetisEncode())
        msg->EnCoding();

    _server->SendPacket(SessionID, msg, bBroadCast, pIDVector, wVectorLen);

}

void clsEchoZone::REQ_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    RES_HEARTBEAT(SessionID, msg);
}

void clsEchoZone::RES_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stHeader header;

    header.byCode = 0x77;
    msg->InitMessage();

    msg->PutData(&header, sizeof(stHeader));
    *msg << wType;
    short *pheaderlen = (short *)(msg->_frontPtr + offsetof(stHeader, sDataLen));
    *pheaderlen = msg->_rearPtr - msg->_frontPtr - _server->GetheaderSize();
    if (_server->GetisEncode())
        msg->EnCoding();
    _server->SendPacket(SessionID, msg, bBroadCast, pIDVector, wVectorLen);
}
