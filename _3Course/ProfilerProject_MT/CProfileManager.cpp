#include "CProfileManager.h"
#include <Windows.h>
#include <strsafe.h>


namespace ProfilerMT
{
    enum
    {
        MAX_THREAD_COUNT = 16,
        MAX_RECORD_COUNT = 16,
        MAX_TAG_NAME_LENGTH = 31 + 1,
        OUTLINER_COUNT = 2, // ?

        MAX_1ST = 0,
        MIN_1ST = 0,
        MAX_2ND = 1,
        MIN_2ND = 1,

    };

    struct ProfileRecord
    {
        struct
        {
            WCHAR Name[MAX_TAG_NAME_LENGTH];
            unsigned int CchLength;
        } Tag;

        bool IsCheckout;
        LARGE_INTEGER StartAt;
        LONGLONG TotalElapsed;
        LONGLONG MaxOutliners[OUTLINER_COUNT];
        LONGLONG MinOutliners[OUTLINER_COUNT];
        ULONGLONG CountOfCall;
    };

    struct stRecordInfo
    {
        stRecordInfo(void)
        {
            Reset();
        }

        void Reset(void)
        {
            size_t recordIndex;

            ThreadId = GetCurrentThreadId();
            RecordCount = 0;

            for (recordIndex = 0; recordIndex < MAX_RECORD_COUNT; recordIndex++)
            {
                ProfileRecord &refRecord = Records[recordIndex];
                ZeroMemory(&refRecord, sizeof(ProfileRecord));

                //min은 최대 값
                refRecord.MinOutliners[MIN_1ST] = MAXLONGLONG;
                refRecord.MinOutliners[MIN_2ND] = MAXLONGLONG;
            }
        }

        DWORD ThreadId;
        ProfileRecord Records[MAX_RECORD_COUNT];
        size_t RecordCount;
    };
    bool Initialization() 
    {
        BOOL retval_QueryPerformanceFrequency;

        s_Tlsidx = TlsAlloc();
        if (s_Tlsidx == TLS_OUT_OF_INDEXES)
        {
            __debugbreak();
            return false;
        }
        retval_QueryPerformanceFrequency = QueryPerformanceFrequency(&s_Frequency);    
        if (retval_QueryPerformanceFrequency == 0)
        {
            __debugbreak();
            return false;
        }

        return true;
    }
    stRecordInfo *GetTlsRecordInfo() 
    {
        return reinterpret_cast<stRecordInfo*>(TlsGetValue(s_Tlsidx));
    }
    stRecordInfo* CreateTlsRecordInfo()
    {
        return new stRecordInfo();
    }
    void Start(const wchar_t const lpTagName)
    {

        WCHAR *pOutTagEnd;
        stRecordInfo *pTlsRecordInfo;


        ProfileRecord record(lpTagName);

        pTlsRecordInfo = GetTlsRecordInfo();
        if (pTlsRecordInfo == nullptr)
            pTlsRecordInfo = CreateTlsRecordInfo();

        pTlsRecordInfo->ThreadId = GetCurrentThreadId();
        pTlsRecordInfo->Records[pTlsRecordInfo->RecordCount] = record;
        
    }

    void End(const wchar_t const lpTagName)
    {

    }
    static bool sbInit = Initialization();

    static LARGE_INTEGER s_Frequency;
    inline static DWORD s_Tlsidx;
}



