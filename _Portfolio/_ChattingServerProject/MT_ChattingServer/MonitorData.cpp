#include "MonitorData.h"
#include "CTestClient.h"

int g_MonitorData[enMonitorType::Max];
HANDLE g_hMonitorEvent = CreateEvent(nullptr, 0, false, nullptr);

void ClientFunc()
{
    CTestClient *client = new CTestClient(true);
    Parser parser;
    parser.LoadFile(L"Config.txt");

    wchar_t ip[16];
    unsigned short port;

    parser.GetValue(L"MonitorServer_IP_Address",ip,16);
    parser.GetValue(L"MonitorServer_IP_Port", port);

    client->Connect(ip, port);
}
