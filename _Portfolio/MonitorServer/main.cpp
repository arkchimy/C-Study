#include "CTestServer.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
CDump dump;

int main()
{
    CDump::SetHandlerDump();

    stWSAData wsa;
    CTestServer server(true);
    wchar_t bindAddr[16] = L"0.0.0.0";

    server.Start(bindAddr, 21350, 0, 5, 0, 1, 10);
    while (1)
    {

    }

    return 0;
}