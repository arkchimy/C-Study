#pragma once
#include <cstdint>

using ContentsQIdx = std::uint32_t;
using SessionIDType = std::uint64_t;

using TimerType = std::uint32_t;
using AccountType = std::uint64_t;

enum class enPlayerState : std::uint8_t
{
    Session,
    Player,
    DisConnect,
    Max,
};

struct stPlayer
{
    void Initalize()
    {
        m_State = enPlayerState::Max;
        m_sessionID = 0;

        m_Timer = 0;
        m_AccountNo = 0;

        iSectorX = 0;
        iSectorY = 0;
    }

    ContentsQIdx m_ContentsQIdx = 0;
    enPlayerState m_State = enPlayerState::Max;
    SessionIDType m_sessionID = 0;

    TimerType m_Timer = 0;
    AccountType m_AccountNo = 0;

    wchar_t m_ID[20]{0};
    wchar_t m_Nickname[20]{0};
    char m_SessionKey[64]{0};

    int iSectorX = 0;
    int iSectorY = 0;

    char m_ipAddress[16];
    unsigned short m_port;
};
