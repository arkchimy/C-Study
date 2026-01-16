#include "clsLoginZone.h"

void clsLoginZone::OnEnterWorld(ull SessionID, SOCKADDR_IN &addr)
{


}

void clsLoginZone::OnRecv(ull SessionID, CMessage *msg)
{
    if (PacketProc(SessionID, msg) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        _server->Disconnect(SessionID);
    }
    totalPacketCnt++;
}

void clsLoginZone::OnUpdate()
{
}

void clsLoginZone::OnLeaveWorld(ull SessionID)
{
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

void clsLoginZone::REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    // 전환 완료 메세지가 없으므로
    _server->RequeseMoveZone(SessionID, (ZoneKeyType)enZoneType::EchoZone);
    
}

void clsLoginZone::RES_LOGIN(ull SessionID, CMessage *msg, BYTE Status, INT64 AccountNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    // 보내고 끊어지는 기능 넣을떄를 기대하고 납둠.
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

void clsLoginZone::REQ_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{

    RES_HEARTBEAT(SessionID, msg);
}

void clsLoginZone::RES_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
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
