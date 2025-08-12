#pragma once
#include "../../error_log.h"
#include <WS2tcpip.h>
#include <WinSock2.h>

#include <Windows.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")
class st_WSAData
{
  public:
    st_WSAData();
    ~st_WSAData();
};

