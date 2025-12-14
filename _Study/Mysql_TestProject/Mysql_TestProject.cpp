#include <iostream>
#include <mysql.h>
#include <errmsg.h>
#include <windows.h>
#include <thread>

//PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;%PATH%

void DB_SaveThread(void *arg);
void EventThread(void *arg);

struct stDBEvent
{
    virtual void exe() = 0;

};
struct stInsert :public stDBEvent
{
    void exe() 
    {

    }
};
//HANDLE WINAPI CreateIoCompletionPort(
//    _In_ HANDLE FileHandle,
//    _In_opt_ HANDLE ExistingCompletionPort,
//    _In_ ULONG_PTR CompletionKey,
//    _In_ DWORD NumberOfConcurrentThreads);
#include <conio.h>

int main()
{
    HANDLE hIocp = nullptr;
    
    hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (hIocp == INVALID_HANDLE_VALUE)
        __debugbreak();

    std::thread DBThread(DB_SaveThread,hIocp);
    SetThreadDescription(DBThread.native_handle(), L"DBThread");

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == VK_ESCAPE)
            {
                CloseHandle(hIocp);
            }
            if (ch == 'a' || ch == 'A')
            {
                PostQueuedCompletionStatus()
            }
        }
    }

}

void DB_SaveThread(void *arg)
{
    MYSQL conn;
    MYSQL *connection = NULL;
    MYSQL_RES *sql_result;
    MYSQL_ROW sql_row;
    int query_stat;

    HANDLE hIOCP = arg;
    // 초기화
    mysql_init(&conn);

    // DB 연결
    connection = mysql_real_connect(&conn, "127.0.0.1", "root", "123123", "Test", 3306, (char *)NULL, 0);
    if (connection == NULL)
    {

        fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
        mysql_errno(&conn);
        __debugbreak();
        return;
    }

    // GQCS 로 할까?  결국 PQCS 로하면 똑같은거고,,,

    DWORD transfferd;
    ULONG64 key;
    OVERLAPPED *overalpped;

    {
        //_Out_ LPDWORD lpNumberOfBytesTransferred,
        //    _Out_ PULONG_PTR lpCompletionKey,
        //    _Out_ LPOVERLAPPED *lpOverlapped,
    }
    while (1)
    {
        {
            transfferd = 0;
            key = 0;
            overalpped = nullptr;
        }
        GetQueuedCompletionStatus(hIOCP, &transfferd, &key, &overalpped, INFINITE);
        if (transfferd == 0 && key == 0 && overalpped == nullptr)
            __debugbreak();
    }
}

void EventThread(void *arg)
{
}
