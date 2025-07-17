#include "stWSAdata.h"
#include "CLanServer.h"
#include <thread>

unsigned AcceptThread(void *arg) 
{
    CLanServer *server = reinterpret_cast<CLanServer *>(arg);

    while (1)
    {
        server->AcceptThread();
        Sleep(0);
    }
}
int main()
{
    stWSAData wsadata;
    CLanServer server(8000);
    _beginthreadex(nullptr, 0, AcceptThread, &server, 0, nullptr);

    while (1)
    {
        server.Update();
        Sleep(0);
    }
}
