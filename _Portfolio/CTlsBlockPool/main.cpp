#include "CTlsBlockPool.h"
#include <Windows.h>
#include <thread>
CTlsBlockPoolManager *manager1;

thread_local CTlsBlockPool msgPool;

int main()
{

    manager1 = new CTlsBlockPoolManager(300,100);

}
