#include "Profiler_lib.h"
#include <strsafe.h>

LARGE_INTEGER Freq;
PROFILE_Manager PROFILE_Manager::Instance;

static FILE *Profile = nullptr;
const WCHAR *formatting[10] = {
    L"Name                 | Average                 | Min                  | "
    L"Max                  | Call                    | 로직 호출 수 / Cnt  | "
    L"소요시간 % ",
    L"%-20ls",
    L"%20.3lf μs",
    L"%20.3lf μs",
    L"%20.3lf μs",
    L"%20zu",   // Call -6
    L"%20.3lf", // 함수 1번에 호출 수
    L"%20.1lf Percent",
    L"\n\t "
    L"========================================================================="
    L"===================== \n"};

#define MicroSecond(x) x * (1e6 / Freq.QuadPart)
void myStrcat(WCHAR *buffer, const WCHAR *str)
{
    // 버퍼 크기가 불충분하다면 버퍼오버런 발생.

    size_t len = wcslen(buffer) + wcslen(str);
    memcpy(&buffer[wcslen(buffer)], str, wcslen(str) * sizeof(WCHAR));

    buffer[len] = 0;
}

bool ExistChk(const wchar_t *tag, const wchar_t *tag2)
{
    if (tag == nullptr)
        return false;
    size_t len = wcslen(tag);
    if (len != wcslen(tag2))
        return false;

    for (size_t current = 0; current < len; current++)
    {
        if (*(tag + current) != *(tag2 + current))
            return false;
    }

    return true;
}

void PROFILE_INFO::writeInfo()
{

    WCHAR buffer[250];

    if (iCall >= 5)
    {
        aver = static_cast<double>(
            (iTotalTime - iMax[0] - iMax[1] - iMin[0] - iMin[1]) /
            double(iCall - 4));
    }
    else
    {
        aver = static_cast<double>(iTotalTime);
    }

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[1],
                     _tag);
    // swprintf(buffer, sizeof(buffer), formatting[1], _tag);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[2],
                     MicroSecond(aver));
    // swprintf(buffer, sizeof(buffer), formatting[2], MicroSecond(aver));
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[3],
                     MicroSecond(iMin[0]));
    // swprintf(buffer, sizeof(buffer), formatting[3], MicroSecond(iMin[0]));
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[4],
                     MicroSecond(iMax[1]));
    // swprintf(buffer, sizeof(buffer), formatting[4], MicroSecond(iMax[1]));
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[5],
                     iCall);
    // swprintf(buffer, sizeof(buffer), formatting[5], iCall);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    fwrite(formatting[8], sizeof(WCHAR), wcslen(formatting[8]), Profile);
    for (size_t idx = 0; idx < PROFILE_FUNCSIZE; idx++)
        stFunctions[idx].writeInfo();
    fwrite(formatting[8], sizeof(WCHAR), wcslen(formatting[8]), Profile);
}

void PROFILE_INFO::resetinfo()
{
    lFlag = false;

    lStartTime.QuadPart = 0;
    _tag = nullptr;
    iTotalTime = 0;

    iMin[0] = INT_MAX;
    iMin[1] = INT_MAX;

    iMax[0] = 0;
    iMax[1] = 0;
    iCall = 0;
    for (size_t idx = 0; idx < PROFILE_FUNCSIZE; idx++)
    {
        stFunctions[idx].resetinfo();
    }
}

void PROFILE_INFO::StartLogic(const wchar_t *TagName)
{
    PROFILE_FUNC *session = nullptr;
    for (size_t i = 0; i < PROFILE_FUNCSIZE; i++)
    {
        if (ExistChk(stFunctions[i]._tag, TagName))
        {
            session = &(stFunctions[i]);
            break;
        }
    }
    if (session == nullptr)
    {
        for (size_t i = 0; i < PROFILE_SIZE; i++)
        {
            if (stFunctions[i].bFlag == false)
            {
                stFunctions[i].bFlag = true;
                session = &(stFunctions[i]);
                break;
            }
        }
    }
    if (session == nullptr)
        __debugbreak();

    session->SetOwnerInfo(this);
    session->iCall++;
    session->_tag = TagName;
    QueryPerformanceCounter(&session->lStartTime);
}

void PROFILE_INFO::EndLogic(const wchar_t *TagName)
{
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    PROFILE_FUNC *session = nullptr;
    for (size_t i = 0; i < PROFILE_SIZE; i++)
    {
        if (ExistChk(stFunctions[i]._tag, TagName))
        {
            session = &stFunctions[i];
            break;
        }
    }
    if (session == nullptr)
        __debugbreak();
    long long current_time = current.QuadPart - session->lStartTime.QuadPart;
    session->iTotalTime += current_time;
    if (session->iMax[1] <= current_time)
    {
        session->iMax[0] = session->iMax[1];
        session->iMax[1] = current_time;
    }
    else if (session->iMax[0] < current_time)
    {
        session->iMax[0] = current_time;
    }
    if (session->iMin[0] >= current_time)
    {
        session->iMin[1] = session->iMin[0];
        session->iMin[0] = current_time;
    }
    else if (session->iMin[1] > current_time)
    {
        session->iMin[1] = current_time;
    }
}

PROFILE_Manager::PROFILE_Manager()
{
    myStrcat(ProfileName, TEXT(__DATE__));
    myStrcat(ProfileName, L".txt");

    QueryPerformanceFrequency(&Freq);

}

void PROFILE_Manager::createProfile()
{
#ifdef PROFILE

    _wfopen_s(&Profile, ProfileName, L"w+,ccs=UTF-16LE");
    if (Profile == nullptr)
    {
        printf("NotFind File\n");
        return;
    }
    fwrite(formatting[0], sizeof(WCHAR), wcslen(formatting[0]), Profile);
    fwrite(L"\n", sizeof(WCHAR), 1, Profile);

    for (int i = 0; i < PROFILE_SIZE; i++)
    {
        if (PROFILE_Manager::Instance.infoPool[i].lFlag)
        {
            PROFILE_Manager::Instance.infoPool[i].writeInfo();
        }
    }

    fflush(Profile);
    fclose(Profile);

#else

#endif
}

void PROFILE_Manager::resetInfo()
{
#ifdef PROFILE

    for (int i = 0; i < PROFILE_SIZE; i++)
    {
        PROFILE_Manager::Instance.infoPool[i].resetinfo();
    }
#else
    {
    }
#endif
}

void PROFILE_FUNC::resetinfo()
{
    bFlag = false;

    lStartTime.QuadPart = 0;
    _tag = nullptr;
    iTotalTime = 0;

    iMin[0] = INT_MAX;
    iMin[1] = INT_MAX;

    iMax[0] = 0;
    iMax[1] = 0;
    iCall = 0;
}

void PROFILE_FUNC::writeInfo()
{
    if (_tag == nullptr)
        return;
    WCHAR buffer[300];

    if (iCall >= 5)
    {
        aver = static_cast<double>(
            (iTotalTime - iMax[0] - iMax[1] - iMin[0] - iMin[1]) /
            double(iCall - 4));
    }
    else
    {
        aver = static_cast<double>(iTotalTime);
    }

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[1],
                     _tag);
    // swprintf(buffer, sizeof(buffer), formatting[1], _tag);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[2],
                     MicroSecond(aver) * (float)iCall / _clpOwnerInfo->iCall);

    // swprintf(buffer, sizeof(buffer), formatting[2],MicroSecond(aver) *
    // (float)iCall / _clpOwnerInfo->iCall);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[3],
                     MicroSecond(iMin[0]));

    // swprintf(buffer, sizeof(buffer), formatting[3], MicroSecond(iMin[0]));
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[4],
                     MicroSecond(iMax[1]));
    // swprintf(buffer, sizeof(buffer), formatting[4], MicroSecond(iMax[1]));
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[5],
                     iCall);
    // swprintf(buffer, sizeof(buffer), formatting[5], iCall);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[6],
                     (float)iCall / _clpOwnerInfo->iCall);

    // swprintf(buffer, sizeof(buffer), formatting[6],(float)iCall /
    // _clpOwnerInfo->iCall);
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), formatting[7],
                     (aver * 100 * (float)iCall / _clpOwnerInfo->iCall) /
                         _clpOwnerInfo->aver);

    /*swprintf(buffer, sizeof(buffer), formatting[7],
             (aver * 100 * (float)iCall / _clpOwnerInfo->iCall) /
                 _clpOwnerInfo->aver);*/
    fwrite(buffer, sizeof(WCHAR), wcslen(buffer), Profile);

    fwrite(L"\n", sizeof(WCHAR), 1, Profile);
}

void Profile::StartLogic(const wchar_t *tag)
{
    for (size_t idx = 0; idx < PROFILE_SIZE; idx++)
    {
        if (ExistChk(PROFILE_Manager::Instance.infoPool[idx]._tag, _tag))
        {
            PROFILE_Manager::Instance.infoPool[idx].StartLogic(tag);
            break;
        }
    }
}

void Profile::EndLogic(const wchar_t *tag)
{
    for (size_t idx = 0; idx < PROFILE_SIZE; idx++)
    {
        if (ExistChk(PROFILE_Manager::Instance.infoPool[idx]._tag, _tag))
        {
            PROFILE_Manager::Instance.infoPool[idx].EndLogic(tag);
            break;
        }
    }
}
