#include <iostream>
#include "clsBlockPool.h"
#include "IJob.h"



clsBlockPool pool;
bool bOn = true;

void Msg1Thread(void* arg)
{
    while (bOn)
    {
        for (int i = 0; i < 10000; i++)
        {
            CreaetMsg *msg = pool.Alloc<CreaetMsg>();
            msg->exe();
            pool.Release<CreaetMsg>(msg);
            break;


        }
    }
}
void Msg2Thread(void* arg)
{
    while (bOn)
    {
        for (int i = 0; i < 10000; i++)
        {
            CreaetMsg2 *msg = pool.Alloc<CreaetMsg2>();
            msg->exe();
            pool.Release<CreaetMsg2>(msg);
            break;
        }

    }
}
#include <conio.h>


int main()
{
    size_t blockSize = sizeof(CreaetMsg2);
    pool.FixedBlockSize(blockSize);

    std::thread th1(Msg1Thread,nullptr);
    std::thread th2(Msg2Thread,nullptr);

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q')
            {
                bOn == false;
            }
        }
    }
    th1.join();
    th2.join();
}
