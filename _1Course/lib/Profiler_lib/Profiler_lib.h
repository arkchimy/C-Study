#pragma once

#include <iostream>
#include <windows.h>

#ifdef PROFILE
#define PRO_BEGIN(TagName)                                                     \
    {                                                                          \
        PROFILE_INFO *info = nullptr;                                          \
        for (size_t i = 0; i < PROFILE_SIZE; i++)                              \
        {                                                                      \
            if (PROFILE_Manager::Instance.infoPool[i]._tag == TagName)         \
            {                                                                  \
                info = &(PROFILE_Manager::Instance.infoPool[i]);               \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (info == nullptr)                                                   \
        {                                                                      \
            for (size_t i = 0; i < PROFILE_SIZE; i++)                          \
            {                                                                  \
                if (PROFILE_Manager::Instance.infoPool[i].lFlag == false)      \
                {                                                              \
                    PROFILE_Manager::Instance.infoPool[i].lFlag = true;        \
                    info = &(PROFILE_Manager::Instance.infoPool[i]);           \
                    break;                                                     \
                }                                                              \
            }                                                                  \
        }                                                                      \
        if (info == nullptr)                                                   \
            __debugbreak();                                                    \
        info->iCall++;                                                         \
        info->_tag = TagName;                                                  \
        QueryPerformanceCounter(&info->lStartTime);                            \
    }
#define PRO_END(TagName)                                                       \
    {                                                                          \
        LARGE_INTEGER current;                                                 \
        QueryPerformanceCounter(&current);                                     \
        PROFILE_INFO *info = nullptr;                                          \
        for (size_t i = 0; i < PROFILE_SIZE; i++)                              \
        {                                                                      \
            if (PROFILE_Manager::Instance.infoPool[i]._tag == TagName)         \
            {                                                                  \
                info = &PROFILE_Manager::Instance.infoPool[i];                 \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (info == nullptr)                                                   \
            __debugbreak();                                                    \
        long long current_time = current.QuadPart - info->lStartTime.QuadPart; \
        info->iTotalTime += current_time;                                      \
        if (info->iMax[1] <= current_time)                                     \
        {                                                                      \
            info->iMax[0] = info->iMax[1];                                     \
            info->iMax[1] = current_time;                                      \
        }                                                                      \
        else if (info->iMax[0] < current_time)                                 \
        {                                                                      \
            info->iMax[0] = current_time;                                      \
        }                                                                      \
        if (info->iMin[0] >= current_time)                                     \
        {                                                                      \
            info->iMin[1] = info->iMin[0];                                     \
            info->iMin[0] = current_time;                                      \
        }                                                                      \
        else if (info->iMin[1] > current_time)                                 \
        {                                                                      \
            info->iMin[1] = current_time;                                      \
        }                                                                      \
    }
#else
#define PRO_BEGIN(TagName)
#define PRO_END(TagName)
#endif
#define PROFILE_SIZE 30
#define PROFILE_FUNCSIZE 30

void myStrcat(WCHAR *buffer, const WCHAR *str);
bool ExistChk(const wchar_t *tag, const wchar_t *tag2);
struct PROFILE_FUNC
{
    PROFILE_FUNC()
        : iCall(0), _tag(nullptr), lStartTime(LARGE_INTEGER()),
          _clpOwnerInfo(nullptr), aver(0.f)
    {
        iMin[0] = INT_MAX;
        iMin[1] = INT_MAX;

        iMax[0] = 0;
        iMax[1] = 0;
    }
    void SetOwnerInfo(class PROFILE_INFO *session) { _clpOwnerInfo = session; }
    void resetinfo();
    void writeInfo();

    bool bFlag = 0; // ���������� ��� ����. (�迭�ÿ���)
    const WCHAR *_tag;

    LARGE_INTEGER lStartTime; // �������� ���� ���� �ð�.

    __int64 iTotalTime = 0; // ��ü ���ð� ī���� Time.	(��½�
                            // ȣ��ȸ���� ������ ��� ����)
    __int64 iMin[2] = {
        INT_MAX, INT_MAX}; // �ּ� ���ð� ī���� Time.	(�ʴ�����
                           // ����Ͽ� ���� / [0] �����ּ� [1] ���� �ּ� [2])
    __int64 iMax[2] = {0, 0}; // �ִ� ���ð� ī���� Time.	(�ʴ�����
                              // ����Ͽ� ���� / [0] �����ִ� [1] ���� �ִ� [2])

    __int64 iCall; // ���� ȣ�� Ƚ��.
    double aver;
    class PROFILE_INFO *_clpOwnerInfo;
};
class PROFILE_INFO
{
  public:
    PROFILE_INFO() : iCall(0), _tag(nullptr), lStartTime(LARGE_INTEGER())
    {
        iMin[0] = INT_MAX;
        iMin[1] = INT_MAX;

        iMax[0] = 0;
        iMax[1] = 0;
        aver = 0;
    }
    ~PROFILE_INFO(){};
    void writeInfo();
    void resetinfo();
    void StartLogic(const wchar_t *TagName);
    void EndLogic(const wchar_t *TagName);

  public:
    long lFlag = 0; // ���������� ��� ����. (�迭�ÿ���)
    const WCHAR *_tag;

    LARGE_INTEGER lStartTime; // �������� ���� ���� �ð�.

    __int64 iTotalTime = 0; // ��ü ���ð� ī���� Time.	(��½�
                            // ȣ��ȸ���� ������ ��� ����)
    __int64 iMin[2] = {
        INT_MAX, INT_MAX}; // �ּ� ���ð� ī���� Time.	(�ʴ�����
                           // ����Ͽ� ���� / [0] �����ּ� [1] ���� �ּ� [2])
    __int64 iMax[2] = {0, 0}; // �ִ� ���ð� ī���� Time.	(�ʴ�����
                              // ����Ͽ� ���� / [0] �����ִ� [1] ���� �ִ� [2])

    __int64 iCall; // ���� ȣ�� Ƚ��.
    double aver;
    PROFILE_FUNC stFunctions[PROFILE_FUNCSIZE];
};

class PROFILE_Manager
{
    friend class PROFIL_INFO;

  public:
    PROFILE_INFO infoPool[PROFILE_SIZE];

  private:
    PROFILE_Manager();
    ~PROFILE_Manager() = default;

  public:
    void createProfile();
    static void resetInfo();

    static PROFILE_Manager Instance;

  private:
    WCHAR ProfileName[21] = L"Profile_";
};

class Profile
{
  public:
    Profile(const WCHAR *tag)
    {
        PRO_BEGIN(tag);
        _tag = tag;
    }
    ~Profile() { PRO_END(_tag); }
    void StartLogic(const wchar_t *tag);
    void EndLogic(const wchar_t *tag);
    const WCHAR *_tag;
};
