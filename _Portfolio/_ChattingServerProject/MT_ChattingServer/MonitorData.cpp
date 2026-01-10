#include "MonitorData.h"
#include "CTestClient.h"

int g_MonitorData[enMonitorType::Max];
HANDLE g_hMonitorEvent = CreateEvent(nullptr, 0, false, nullptr);

void ClientFunc()
{
    CTestClient *client = new CTestClient(true);
    wchar_t ip[] = L"127.0.0.1";

    client->Connect(ip, 21350);
}
