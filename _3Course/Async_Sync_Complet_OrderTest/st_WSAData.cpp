#include "st_WSAData.h"


st_WSAData::st_WSAData()
{
    WSAData wsa;
    DWORD wsaStartRetval;

    wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaStartRetval != 0)
    {
        ERROR_FILE_LOG(L"WSAData_Error.txt",L"WSAStartup retval is not Zero \n");
        __debugbreak();
    }
}

st_WSAData::~st_WSAData()
{
    WSACleanup();
}
