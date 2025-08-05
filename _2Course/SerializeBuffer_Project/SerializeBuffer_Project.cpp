#include "CMessage.h"
#include <iostream>
#include <vector>

struct st_Header
{
    DWORD type;
    DWORD len;
};

int main()
{
    int a;
    int b;
    int c, d;



    std::vector<CMessage *> vec;

    st_Header header,temp;
    DWORD loopCnt;
    ULONG_PTR cnt;
    while (1)
    {
        
        loopCnt = rand() % 1000;
        cnt = loopCnt;
        vec.clear();
        vec.reserve(loopCnt);
        printf("Msg Count : %llu \n", cnt);
        for (DWORD i = 0; i < loopCnt; i++)
        {
            CMessage *msg = new CMessage();
            vec.push_back(msg);

            header.type = rand();
            header.len = 8;

            a = rand();
            b = rand();

            try
            {
                msg->PutData((char *)&header, sizeof(header));
                *msg << a << b;
            }
            catch (const MessageException &ex)
            {
                switch (ex.type())
                {

                case MessageException::HasNotData:
                case MessageException::NotEnoughSpace:
                    ERROR_FILE_LOG(ex.what());
                    break;
                default:
                    __debugbreak();
                }
            }
            try
            {
                msg->GetData((char *)&temp, sizeof(st_Header));
                *msg >> c >> d;
                if (a + b != c + d)
                    __debugbreak();
            }
            catch (const MessageException &ex)
            {
                switch (ex.type())
                {

                case MessageException::HasNotData:
                case MessageException::NotEnoughSpace:
                    ERROR_FILE_LOG(ex.what());
                    break;
                default:
                    __debugbreak();
                }
            }
            cnt--;
            //printf("Msg Count : %llu \n", cnt);
        }
        for (DWORD i = 0; i < loopCnt; i++)
        {
            delete vec[i];
        }
        printf("Msg Count : %llu \n", cnt);
        
    }
}
