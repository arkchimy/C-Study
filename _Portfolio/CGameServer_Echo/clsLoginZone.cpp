#include "clsLoginZone.h"

#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")

thread_local cpp_redis::client *client;
thread_local bool bRedisOnce ;

void clsLoginZone::OnEnterWorld(ull SessionID, SOCKADDR_IN &addr)
{
    // 한번 만.
    if (bRedisOnce == false)
    {
        bRedisOnce = true;
        client = new cpp_redis::client();

        client->connect(_server->RedisIpAddress, 6379);
    }

    stPlayer *player;

    {
        std::lock_guard<SharedMutex> lock(_SessionTable_Mutex);
        auto prePlayerIter = prePlayer_hash.find(SessionID);
        auto sessionIDIter = SessionID_hash.find(SessionID);

        // 이미 있었다면 문제임
        if (prePlayerIter != prePlayer_hash.end() || sessionIDIter != SessionID_hash.end())
        {
            __debugbreak();
        }

        player = (stPlayer *)player_pool.Alloc();

        player->_lastRecvTime = timeGetTime();
        player->_addr = addr;
        player->_SessionID = SessionID;

        prePlayer_hash.insert({SessionID, player});
    }
}

void clsLoginZone::OnRecv(ull SessionID, CMessage *msg)
{
    auto iter = prePlayer_hash.find(SessionID);
    if (iter == prePlayer_hash.end())
    {
        stTlsObjectPool<CMessage>::Release(msg);
        _server->Disconnect(SessionID);
        return;
    }
    if (PacketProc(SessionID, msg) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        _server->Disconnect(SessionID);
    }
    iter->second->_lastRecvTime = timeGetTime();

    totalPacketCnt++;
}

void clsLoginZone::OnUpdate()
{
    DWORD currentTime = timeGetTime();
    DWORD distance;

    //TODO : 하트비트 현재 하고있지않음.
    //for (auto &iter : prePlayer_hash)
    //{
    //    stPlayer *player = iter.second;
    //    distance = currentTime - player->_lastRecvTime;
    //    if (distance >= _sessionTimeoutMs)
    //    {
    //        _server->Disconnect(player->_SessionID);
    //    }
    //}
}

void clsLoginZone::OnLeaveWorld(ull SessionID)
{
    auto iter = prePlayer_hash.find(SessionID);

    // 없다면 문제임
    if (iter == prePlayer_hash.end())
    {
        __debugbreak();
    }
    prePlayer_hash.erase(iter);
}

void clsLoginZone::OnDisConnect(ull SessionID)
{
    stPlayer *player;

    auto iter = prePlayer_hash.find(SessionID);
    //CSystemLog::GetInstance()->Log(L"OnDisConnect", en_LOG_LEVEL::ERROR_Mode, L"LoginZone_DisConnect %lld", SessionID);
    // 없다면 문제임
    if (iter == prePlayer_hash.end())
    {
        __debugbreak();
    }
    player = iter->second;
    // 중복제거로인한 다른 sessionID가 들어가있을수 있음.
    if (player->_SessionID == SessionID)
        prePlayer_hash.erase(iter);
    {
        std::lock_guard<SharedMutex> lock(_SessionTable_Mutex);
        auto iter = SessionID_hash.find(SessionID);
        if (iter == SessionID_hash.end())
        {
            // 삭제하려는데 없음.
            // LoginPacket을 보내기전에 끊은 경우. 정상인 케이스
            // __debugbreak();
        }
        else
        {
            SessionID_hash.erase(iter);
        }
        // Player반환은 여기서.
        player_pool.Release(player);
    }
}

bool clsLoginZone::PacketProc(ull SessionID, CMessage *msg)
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
    case en_PACKET_CS_GAME_REQ_LOGIN:
    {
        try
        {
            INT64 AccountNo;
            WCHAR SessionKey[32];
            *msg >> AccountNo;
            msg->GetData(SessionKey, 64);
            REQ_LOGIN(SessionID, msg, AccountNo, SessionKey);
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
        _msgTypeCntArr[LoginPacket]++;
        break;
    }

    default:
        return false;
    }

    return true;
}

void clsLoginZone::REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stPlayer *player;

    {
        // Login응답.
        // redis에서 읽기, 가져오고 token을 비교 같다면

        std::string key = std::to_string(AccountNo);
        auto future = client->get(key.c_str());
        client->sync_commit();
        cpp_redis::reply reply = future.get();

        if (reply.is_null())
        {
            //TODO : Token 현재 비교안함.
            // 
            //printf("AccountNo %lld not found in redis\n", AccountNo);
            //__debugbreak();
            //return;
        }
        else
        {
            std::string sessionKey = reply.as_string();

            char SessionKeyA[66];
            memcpy(SessionKeyA, SessionKey, 64);
            SessionKeyA[64] = '\0';
            SessionKeyA[65] = '\0';

            key = std::to_string(AccountNo);

            if (sessionKey.compare(SessionKeyA) != 0)
            {
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %10s %12s %10s ",
                                               L"LoginError - hash is Not Equle: ", SessionKeyA,
                                               L"현재들어온ID:", sessionKey);

                stTlsObjectPool<CMessage>::Release(msg);
                _server->Disconnect(SessionID);
                return;
            }
        }
    }

    {
        auto iter = Account_hash.find(AccountNo);
        // 중복 로그인이라면 있던 player를 끊음.
        if (iter != Account_hash.end())
        {
            player = iter->second;
            _server->Disconnect(player->_SessionID);
            Account_hash.erase(iter);
        }

    }

    {
        auto prePlayeriter = prePlayer_hash.find(SessionID);
        if (prePlayeriter == prePlayer_hash.end())
        {
            //  LoginPacket를 두번 보낸 공격일 수 있음.
            // TODO : 공격이 아니라면 있을 수 없으므로 일단은 납둠.
            __debugbreak();
        }
        player = prePlayeriter->second;
        player->_AccountNo = AccountNo;
        Account_hash.insert({AccountNo, player});

        {
            std::lock_guard<SharedMutex> lock(_SessionTable_Mutex);

            // 전환 완료 메세지가 없으므로
            auto iter = SessionID_hash.find(SessionID);
            if (iter != SessionID_hash.end())
            {
                // 이제 추가하는 것이라 있으면 안됨.
                __debugbreak();
            }
            SessionID_hash.insert({SessionID, player});
            _server->RequeseMoveZone(SessionID, (ZoneKeyType)enZoneType::EchoZone);
        }
    }
}
