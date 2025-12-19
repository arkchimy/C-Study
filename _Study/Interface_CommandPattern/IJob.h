#pragma once
#include "clsBlockPool.h"


// Pool은 한 개 이지만, placementNew 을 이용하여 하자.
// HeapCreate를 이용하여 해당 풀에서 문제가 발생한다면 해당 Pool의 문제로 취급하자. 


class IJob
{
  protected:

    virtual void exe() = 0;
};

class CreaetMsg : public IJob
{
    void *operator new(size_t size)
    {
       
    }
    void exe()
    {
        printf("CreaetMsg exe \n");
    }
    int AccountNo;
};

class CreaetMsg2 : public IJob
{
    void *operator new(size_t size)
    {
      
    }
    void exe()
    {
        printf("CreaetMsg2 exe \n");
    }
    int AccountNo;
    char buffer[100];
};
