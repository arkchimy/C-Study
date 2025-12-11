// RedisTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")

#include <conio.h>


int main()
{
    WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);

    cpp_redis::client client;

    client.connect("10.0.2.2", 6379);

    std::string key, value;
    char ch;
    while (1)
    {
        std::cout << " Mode [ Add: 'a'  Seach : 's']\n";
        ch = _getch();

        if (ch == 'A' || ch == 'a')
        {
            int ttl_ms = 6000;
            std::cout << " ======================================== \n";
            std::cout << " Search [ Key ]  [ value ] \n";
            std::cin >> key >> value;
            client.psetex(key, ttl_ms, value);
            std::cout << " ======================================== \n";
        }
        if (ch == 'S' || ch == 's')
        {
            std::cout << " ======================================== \n";
            std::cout << " Search [ Key ]  [ value ] \n";
            std::cin >> key;
            client.get(key, [](cpp_redis::reply &reply)
                       { std::cout << reply << std::endl; });
            client.sync_commit();
            std::cout << " ======================================== \n";
        }
    }
}

    //! also support std::future
    //! std::future<cpp_redis::reply> get_reply = client.get("hello");

    //! or client.commit(); for asynchronous call}
