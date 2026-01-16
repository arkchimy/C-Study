#pragma once
#include <timeapi.h>

using ZoneKeyType = uint8_t;

#ifndef ONCE_enZoneType
    #define ONCE_enZoneType
    enum class enZoneType : ZoneKeyType
    {
        LoginZone = 0,
        EchoZone
    };
    struct stPlayer
    {
        INT64 _AccountNo;
        ull _SessionID;
        DWORD _lastRecvTime;

        SOCKADDR_IN _addr;
    };
#endif
