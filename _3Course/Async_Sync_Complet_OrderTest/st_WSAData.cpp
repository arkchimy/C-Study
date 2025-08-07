#include "st_WSAData.h"
#include <WinSock2.h>
#include "../../error_log.h"

st_WSAData::st_WSAData()
{
    WSAData wsa;
    DWORD wsaStartRetval;

    wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaStartRetval != 0)
    {
        ERROR_FILE_LOG("WSAStartup retval is not Zero \n");
        __debugbreak();
    }
}

st_WSAData::~st_WSAData()
{
    WSACleanup();
}
