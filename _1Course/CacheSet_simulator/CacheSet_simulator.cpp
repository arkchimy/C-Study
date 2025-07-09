// CacheSet_simulator.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

#include <Windows.h>
#include <sysinfoapi.h>

#include <math.h>
#include <vector>

class CacheManager
{
  public:
    static CacheManager *GetInstance()
    {
        static CacheManager instance;
        return &instance;
    }

  public:
    struct stCacheLine
    {
        bool valid = false; // 해당 캐시히트여부 판정
        size_t tag = 0;     // 64bit에서는 8바이트 ,x86에서는 4바이트의 크기를 갖는 자료형
        BYTE data[64];
    };
    struct stCacheSet
    {
        stCacheSet()
        {
            _line = new stCacheLine[CacheManager::cache_Way];
        }
        ~stCacheSet()
        {
            delete[] _line;
        }
        stCacheLine &LookupCache(size_t tag)
        {
            stCacheLine *ptr = _line;
            for (BYTE i = 0; i < CacheManager::cache_Way; i++)
            {
                ptr = ptr + i;

                if (ptr->tag == tag && ptr->valid == true)
                {
                    return *ptr;
                }
            }
            return *CacheManager::GetInstance()->invalid_cache_line;
        }
        stCacheLine *_line = nullptr;
    };

    stCacheLine &LookupCache(size_t addr)
    {
        stCacheLine temp;

        size_t index;
        size_t tag;
        index = (addr & cacheIndexMask) >> cacheOffsetBit;
        tag = (addr & cacheTagMask) >> cacheOffsetBit >> cacheIndexBit;

        return cache[index].LookupCache(tag);
    }

  private:
    CacheManager()
    {
        initialize();
        cache = new stCacheSet[cache_Size / cache_Line / cache_Way];
        invalid_cache_line = new stCacheLine;
    }
    ~CacheManager()
    {
        delete[] cache;
    }
    void initialize()
    {

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
        DWORD len = 0;
        GetLogicalProcessorInformation(infos, &len);

        infos = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(len);
        if (infos == nullptr)
            __debugbreak();
        GetLogicalProcessorInformation(infos, &len);

        DWORD cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문
        for (int i = 0; i < cnt; i++)
        {
            if (infos[i].Cache.Level == 1 && infos[i].Cache.Size != 0)
            {
                cache_Line = infos[i].Cache.LineSize;
                cache_Size = infos[i].Cache.Size;
                cache_Way = infos[i].Cache.Associativity;

                cacheOffsetBit = log2(cache_Line);
                cacheIndexBit = log2(cache_Size / cache_Line / cache_Way);

                printf(" cache_Line %d  cache_Size : %d   cache_Way : %d  \n ", cache_Line, cache_Size, cache_Way);
                printf(" cacheOffsetBit %zu \n ", cacheOffsetBit);
                printf(" cacheIndexBit %zu \n ", cacheIndexBit);

                cacheOffsetMask = cache_Line - 1;

                cacheIndexMask = pow(2, cacheIndexBit) - 1;
                cacheIndexMask = cacheIndexMask << cacheOffsetBit;

                DWORD totalbit = sizeof(size_t) * 8;

                cacheTagMask = pow(2, cacheIndexBit + cacheOffsetBit) - 1;
                cacheTagMask = ~cacheTagMask;
                return;
            }
        }
    }

  private:
    inline static WORD cache_Line;
    inline static DWORD cache_Size;
    inline static BYTE cache_Way;

    stCacheLine *invalid_cache_line;

    size_t cacheOffsetBit = 0;
    size_t cacheIndexBit = 0;

    size_t cacheIndexMask = 0;
    size_t cacheOffsetMask = 0;
    size_t cacheTagMask = 0;

    stCacheSet *cache;
};

int main()
{

    char *ptr = (char *)malloc(10);
    if (ptr == nullptr)
        __debugbreak();
    memset(ptr, 0xee, 9);
    ptr[9] = 0;

    auto result = CacheManager::GetInstance()->LookupCache(size_t(ptr));
    memset(result.data, 0xff, 64);

    return 0;
}
