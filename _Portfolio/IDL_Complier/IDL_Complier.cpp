#include <iostream>
#include <strsafe.h>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include <vector>
#include <sstream>

const std::wstring needle1 = L"SizedArray'";
const std::wstring needle2 = L"DynamicArray'";

wchar_t TargetPath[FILENAME_MAX];
WORD TypeOffset;

#define BufferMax 700
#define PackStartFormat L"\n\t const static BYTE %s \t=\t %s ;"

#define CMessageDefaultFormat L"\tconst BYTE %s \t= \t (%s + %d) ;" // 1 씩증가
#define CMessageCustomFormat L"\tconst BYTE %s \t= \t  %s      ;"   // 직접 정의

#define InitCommonFORMAT L"#pragma once\n#define WIN32_LEAN_AND_MEAN \n#include <Windows.h>\n#ifndef CONSTANTS_H \n #define CONSTANTS_H \n"
#define CommonFORMAT L"\t %s \n"
#define CLOSECommonFORMAT L"#endif // CONSTANTS_H \n"
//======================================================================
// Proxy
#define InitProxyFORMAT                                                                      \
    L"\nclass Proxy \n{\n public : \n"
#define CloseProxyFORMAT L"};\n "
#define ProxyFORMAT L"\t void %s; \n"

#define ProxyCPPStart_FORMAT \
    L"\nvoid Proxy::%s \n \
{ \n \
\t stHeader header; \n \
\t CTestServer *server; \n \
\t server = static_cast<CTestServer *>(this); \n \
\n\
\t header.byCode = 0x77; \n \
\t msg->InitMessage(); \n \
\t \n \
\t msg->PutData(&header,sizeof(stHeader)); \n\
"

#define ProxyCPPArg_FORMAT L" << %s;\n"
#define ProxyCPPArg_FORMAT2 L"\t msg->PutData(%s,%d);\n"
#define ProxyCPPArg_FORMAT3 L"\t msg->PutData(%s,%s);\n"

#define ProxyCPPClose_FORMAT \
    L"\
\t short *pheaderlen = (short *)(msg->_frontPtr + offsetof(stHeader, sDataLen));\n\
\t *pheaderlen = msg->_rearPtr - msg->_frontPtr - server->headerSize;\n\
\t if (server->bEnCording)\n\
\t \t msg->EnCoding(); \n \
\t server->SendPacket(SessionID, msg, bBroadCast , pIDVector , wVectorLen );\n};\n\n \
"
// Proxy 끝
//======================================================================

//===================================================================
// Stub 시작
std::vector<wchar_t *> gDefineVec;

std::vector<wchar_t *> gCommonVec; // Common.h 에 들어가야할 것들.

std::vector<wchar_t *> gRPCvec;          // 변환된 함수명이 들어가있는 곳
std::vector<wchar_t *> gRPCvec_Notrance; // Proxy나 stub의 구분 없이. 함수명이 들어가있는 곳


// 함수 선언부를 통해 매개변수들을 사용해야 함.
std::vector<wchar_t *> packStartnum; // 임시 변수 저장

bool LoadFile(wchar_t **buffer);
wchar_t *WordSplit(wchar_t *left, wchar_t *right, wchar_t *limit,
                   wchar_t **temp);

wchar_t *GlobalPacketSplit(wchar_t *left, wchar_t *right, wchar_t *limit,
                           wchar_t **temp);
wchar_t *FuncMaker(wchar_t *left, wchar_t *right, wchar_t *limit,
                   wchar_t **temp);

void MakeCommon();
void MakeProxy();
void MakeStub();

void Replace_CustomeType(std::wstring &str, std::wstring target , std::wstring replace) 
{

    size_t pos;
    {
        // target'를 찾기.
        pos = str.find(target);
        while (pos != std::wstring::npos)
        {
 
            if (pos != std::wstring::npos)
            {
                size_t numStart = pos + target.size();
                size_t numEnd = str.find(L'\'', numStart); // 닫는 따옴표
                str.replace(pos, numEnd - pos + 1, replace); // 포함하여 교체
            }
            pos = str.find(target);
        }
    }
    
}
long GetSize_ForSizeArray(const std::wstring &s)
{
    std::wistringstream iss(s);
    std::wstring retval;
    std::wstring tok;

    while (std::getline(iss, tok, L'\''))
    {
        retval = tok;
    }
    return std::wcstol(retval.c_str(), nullptr, 10);
}

int main()
{
    wchar_t *buffer;
    if (LoadFile(&buffer) == false)
    {
        wprintf_s(L"LoadFile Failed \n");
    }
    wchar_t *limit = buffer + wcslen(buffer) + 1;

    wchar_t *left = buffer, *right = buffer;
    wchar_t *temp;

    while (left <= limit)
    {
        left = WordSplit(left, right, limit, &temp);
        right = left;
        wprintf_s(L"%s\n", temp);
    }
    MakeCommon();
    MakeProxy();
    MakeStub();
}

bool LoadFile(wchar_t **buffer)
{
    FILE *file;

    fopen_s(&file, "IDL.txt", "r,ccs = UTF-16LE");
    if (file == nullptr)
        return false;

    fseek(file, 0, SEEK_END);
    int len = ftell(file);

    fseek(file, 0, SEEK_SET);
    *buffer = (wchar_t *)malloc(len);
    if (*buffer == nullptr)
        return false;

    fread_s(*buffer, len, 1, len, file);

    fclose(file);

    return true;
}

wchar_t *WordSplit(
    wchar_t *left, wchar_t *right, wchar_t *limit,
    wchar_t **temp) // 동적할당을 안에서 해주는 함수 사용이 끝나면 Free해야함.
{
    static bool bChk = false; // { 의 체크
    static bool bEquleChk = false;
    static int cnt = 0;

    while (1)
    {
        if (*right == L'{')
        {
            bChk = true;
        }
        else if (*right == L'}')
        {
            bChk = false;
            packStartnum.pop_back();
        }
        else if (*right == L'=')
        {
            bEquleChk = true;
        }
        else if (*right == L'/')
        {
            left = right;
            while (*right != L'\n')
            {
                right++;
            }
            *temp = (wchar_t *)malloc(2);
            if (*temp == nullptr)
                __debugbreak();
            (*temp)[0] = L'\0';
            return right;
        }
        else if (*right == L'#')
        {
            left = right;
            while (1)
            {
                if (limit <= right)
                    break;
                if (*right == L'\n')
                    break;
                right++;
            }
            *temp = (wchar_t *)malloc((right - left) * sizeof(wchar_t) + 2);
            if (*temp == nullptr)
                __debugbreak();
            memcpy_s(*temp, (right - left) * sizeof(wchar_t), left,
                     (right - left) * sizeof(wchar_t));
            (*temp)[right - left] = L'\0';
            gDefineVec.push_back(*temp);
            return right;
        }
        // 문자의 시작점에 left 놓기.
        if ((L'A' <= *right && *right <= L'Z') ||
            (L'a' <= *right && *right <= L'z') ||
            (L'0' <= *right && *right <= L'9') || *right == L'=')

        {
            left = right;
            break;
        }
        right++;
    }
    // 문자의 끝점에 right 놓기.
    while (1)
    {
        if (right >= limit)
            break;

        if (*right == L' ' || *right == L'\t' || *right == L'\n')
        {
            *temp = (wchar_t *)malloc((right - left) * sizeof(wchar_t) + 2);
            if (*temp == nullptr)
                __debugbreak();
            memcpy_s(*temp, (right - left) * sizeof(wchar_t) + 2, left,
                     (right - left) * sizeof(wchar_t));
            (*temp)[right - left] = L'\0';

            break;
        }
        right++;
    }
    if (bChk == true)
    {
        if (packStartnum.size() == 0)
            __debugbreak();
        wchar_t *equltChk = right;
        wchar_t buffer[BufferMax] = {
            0,
        };

        while (1)
        {
            if (*equltChk == L'\n')
            {
                StringCchPrintfW(buffer, BufferMax, CMessageDefaultFormat,
                                 *temp, packStartnum[0], cnt++);
                break;
            }
            else if (*equltChk == L'=')
            {
                wchar_t *val;
                left = equltChk + 1;
                right = left;

                while (1)
                {
                    if (*right == L'\n')
                    {
                        val = (wchar_t *)malloc((right - left + 1) *
                                                sizeof(wchar_t));
                        if (val == nullptr)
                            __debugbreak();
                        memcpy(val, left, (right - left) * sizeof(wchar_t));
                        val[right - left] = L'\0';
                        break;
                    }
                    right++;
                }
                StringCchPrintfW(buffer, BufferMax, CMessageCustomFormat, *temp,
                                 val);
                packStartnum.pop_back();
                packStartnum.push_back(val);
                cnt = 0;
                break;
            }
            equltChk++;
        }

        free(*temp);
        *temp = (wchar_t *)malloc((wcslen(buffer) + 1) * sizeof(wchar_t));
        if (*temp == nullptr)
            __debugbreak();
        memcpy(*temp, buffer, sizeof(wchar_t) * wcslen(buffer) + 2);
        gCommonVec.push_back(*temp);
    }
    // 뜯어낸 단어가 global 일때 Common.h에 작성되야하는 목록 초기화
    else if (wcscmp(L"global", *temp) == 0)
    {
        left = GlobalPacketSplit(left, right, limit, temp);
        right = left;
    }
    // 뜯어낸 단어가 void일때  gRPCvec에 작성 되어야하는 목록
    else if (wcscmp(L"void", *temp) == 0)
    {
        left = FuncMaker(left, right, limit, temp);
        right = left;
    }
    return right;
}

wchar_t *GlobalPacketSplit(wchar_t *left, wchar_t *right, wchar_t *limit,
                           wchar_t **temp)
{
    free(*temp);

    wchar_t buffer[BufferMax];

    wchar_t *key;
    wchar_t *equle;
    wchar_t *val;

    left = WordSplit(left, right, limit, &key);
    right = left;
    packStartnum.emplace_back(key);

    left = WordSplit(left, right, limit, &equle);
    right = left;

    left = WordSplit(left, right, limit, &val);
    right = left;

    StringCchPrintfW(buffer, BufferMax, PackStartFormat, key, val);

    *temp = (wchar_t *)malloc(sizeof(wchar_t) * (wcslen(buffer) + 1));
    memcpy(*temp, buffer, sizeof(wchar_t) * (wcslen(buffer) + 1));
    free(equle);
    free(val);

    gCommonVec.push_back(*temp);

    return right;
}

wchar_t *FuncMaker(wchar_t *left, wchar_t *right, wchar_t *limit,
                   wchar_t **temp)
{
    
    left = right;
    // 세미콜론
    while (1)
    {
        if (*right == L';')
        {
            break;
        }
        right++;
    }
    *temp = (wchar_t *)malloc((right - left + 1) * sizeof(wchar_t));
    if (*temp == nullptr)
        __debugbreak();
    memcpy(*temp, left, (right - left) * sizeof(wchar_t));
    (*temp)[right - left] = L'\0';
    std::wstring str(*temp);
    std::wstring replace(L"WCHAR*");

    Replace_CustomeType(str, needle1, replace); 
    Replace_CustomeType(str, needle2, replace); 
    // void Fname (int a, int b , int c); 가 저장 됨.

    size_t len = (str.size() + 1) * 2; 

    wchar_t *ptr = (wchar_t *)malloc(len);
    if (ptr == nullptr)
        __debugbreak();

    memcpy(ptr, str.c_str(), len);
    ptr[len/2] = L'\0';

    gRPCvec.push_back(ptr);
    gRPCvec_Notrance.push_back(*temp);

  
    
    return right;
}

void MakeCommon()
{
    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"TargetPath", TargetPath, sizeof(TargetPath));
        parser.GetValue(L"TypeOffset", TypeOffset);

    }
    FILE *file;
    wchar_t route[FILENAME_MAX];

    StringCchPrintfW(route, FILENAME_MAX, TargetPath, L"CommonProtocol.h");
    _wfopen_s(&file, route, L"w ,ccs = UTF-16LE");
    if (file == nullptr)
        __debugbreak();

    wchar_t buffer[BufferMax];

    fwrite(InitCommonFORMAT, 2, wcslen(InitCommonFORMAT), file);

    for (wchar_t *data : gDefineVec)
    {
        StringCchPrintfW(buffer, BufferMax, CommonFORMAT, data);
        fwrite(buffer, 2, wcslen(buffer), file);
    }

    for (wchar_t *data : gCommonVec)
    {
        StringCchPrintfW(buffer, BufferMax, CommonFORMAT, data);
        fwrite(buffer, 2, wcslen(buffer), file);
    }
    fwrite(CLOSECommonFORMAT, 2, wcslen(CLOSECommonFORMAT), file);
    fclose(file);
}

void MakeProxy()
{
    FILE *file;
    wchar_t route[FILENAME_MAX];

    // Proxy.h
    {
        StringCchPrintfW(route, FILENAME_MAX, TargetPath, L"Proxy.h");
        _wfopen_s(&file, route, L"w, ccs = UTF-16LE");
        if (file == nullptr)
            __debugbreak();

        wchar_t buffer[BufferMax];
        {
            wchar_t Value[BufferMax];
            wchar_t headerTag[BufferMax];

            Parser parser;
            parser.LoadFile(L"Config.txt");

            //ProxyHeader 작성.
            int Include_ProxyHeaderCnt;
            parser.GetValue(L"Include_ProxyHeaderCnt", Include_ProxyHeaderCnt);
            for (int i = 1; i <= Include_ProxyHeaderCnt; i++)
            {
                StringCchPrintfW(headerTag, BufferMax, L"Include_ProxyHeader%d", i);
                parser.GetValue(headerTag, Value, BufferMax);

                StringCchPrintfW(buffer, BufferMax, L"%s\n", Value);
                fwrite(buffer, 2, wcslen(buffer), file);
            }
            
        }
 
        fwrite(InitProxyFORMAT, 2, wcslen(InitProxyFORMAT), file);

        for (wchar_t *data : gRPCvec)
        {
            std::wstring str(data);
            if (str.find(L"REQ") != std::wstring::npos)
                continue;

            StringCchPrintfW(buffer, BufferMax, ProxyFORMAT, data);
            fwrite(buffer, 2, wcslen(buffer), file);
        }

        fwrite(CloseProxyFORMAT, 2, wcslen(CloseProxyFORMAT), file);

        fclose(file);
    }

    // Proxy.cpp
    {

        StringCchPrintfW(route, FILENAME_MAX, TargetPath, L"Proxy.cpp");
        _wfopen_s(&file, route, L"w, ccs = UTF-16LE");
        if (file == nullptr)
            __debugbreak();

        wchar_t buffer[BufferMax];
        Parser parser;
        parser.LoadFile(L"Config.txt");

        // ProxyCpp 작성.
        {
            wchar_t Value[BufferMax];
            wchar_t headerTag[BufferMax];

            int Include_ProxyCppCnt;
            parser.GetValue(L"Include_ProxyCppCnt", Include_ProxyCppCnt);
            for (int i = 1; i <= Include_ProxyCppCnt; i++)
            {
                StringCchPrintfW(headerTag, BufferMax, L"Include_ProxyCpp%d", i);
                parser.GetValue(headerTag, Value, BufferMax);

                StringCchPrintfW(buffer, BufferMax, L"%s\n", Value);
                fwrite(buffer, 2, wcslen(buffer), file);
            }
        }

        size_t gRPCidx = 0;
        size_t i = 0;
        std::vector<wchar_t *> types;
        std::vector<wchar_t *> words;

        for (wchar_t *data : gRPCvec)
        {
          
            size_t rmData = wcslen(data);
            memcpy(buffer, data, wcslen(data) * sizeof(wchar_t));
            buffer[rmData] = L'\0';

            size_t oriLen = wcslen(data);
            size_t currentIdx = 0;
            i = 0;
            while (1)
            {

                if (buffer[i] == L'=')
                {
                    while (1)
                    {
                        if (buffer[i] == L',' || buffer[i] == L')')
                            break;
                        i++;
                    }
                    buffer[currentIdx++] = L' ';
                }
                buffer[currentIdx++] = buffer[i++];
                if (i >= oriLen)
                    break;
            }
            buffer[currentIdx++] = L'\0';
            // 함수 구현부에 작성될 buffer;
            wchar_t buffer2[BufferMax];
            wchar_t *data= gRPCvec_Notrance[gRPCidx++];
            
            types.clear();
            words.clear();
            i = 0;

            rmData = wcslen(data);
            memcpy(buffer2, data, wcslen(data) * sizeof(wchar_t));
            buffer2[rmData] = L'\0';

            oriLen = wcslen(data);
            currentIdx = 0;

            while (1)
            {

                if (buffer2[i] == L'=')
                {
                    while (1)
                    {
                        if (buffer2[i] == L',' || buffer2[i] == L')')
                            break;
                        i++;
                    }
                    buffer2[currentIdx++] = L' ';
                }
                buffer2[currentIdx++] = buffer2[i++];
                if (i >= oriLen)
                    break;
            }
            buffer2[currentIdx++] = L'\0';

            wchar_t *left = buffer2, *right = buffer2;
            wchar_t *limit = buffer2 + wcslen(buffer2);

            bool bChk = false;
            size_t cnt = 0;
            while (1)
            {

                if (right >= limit)
                    break;

                while (bChk == false)
                {
                    if (*left == L'(')
                    {
                        bChk = true;
                        left++;
                        right = left;
                        break;
                    }
                    left++;
                }
                wchar_t *temp;
                left = WordSplit(left, right, limit, &temp);
                right = left;
                if (*temp == L',')
                    continue;
                cnt++;
                if (cnt % 2 == 1)
                    types.push_back(temp);
                if (cnt % 2 == 0)
                    words.push_back(temp);
            }
            std::wstring str(buffer);
            if (str.find(L"REQ") != std::wstring::npos)
                continue;
            StringCchPrintfW(buffer2, BufferMax, ProxyCPPStart_FORMAT, buffer);
            fwrite(buffer2, 2, wcslen(buffer2), file);
        
        
        
            {
                std::vector<wchar_t *>::iterator iter = words.end() - TypeOffset;
                fwrite(L"\t *msg ", 2, wcslen(L"\t *msg "), file);
                StringCchPrintfW(buffer, BufferMax, ProxyCPPArg_FORMAT, *iter);
                fwrite(buffer, 2, wcslen(buffer), file);
            }
            //

            i = 2;
            for (std::vector<wchar_t *>::iterator iter = words.begin() + 2;
                 iter != words.end() - TypeOffset; iter++)
            {
                std::wstring temp(types[i]);

                if (temp.find(needle1) != std::wstring::npos)
                {
                    long arraySize = GetSize_ForSizeArray(temp);
                    StringCchPrintfW(buffer, BufferMax, ProxyCPPArg_FORMAT2,
                                        words[i], arraySize * 2);
                }
                else if (temp.find(needle2) != std::wstring::npos)
                {
                    long arraySize = GetSize_ForSizeArray(temp);
                    StringCchPrintfW(buffer, BufferMax, ProxyCPPArg_FORMAT3,
                                        words[i], words[i - 1]);
                }
                else
                {

                    fwrite(L"\t *msg ", 2, wcslen(L"\t *msg "), file);
                    StringCchPrintfW(buffer, BufferMax, ProxyCPPArg_FORMAT, *iter);
                }
                fwrite(buffer, 2, wcslen(buffer), file);
                i++;
            }
            fwrite(ProxyCPPClose_FORMAT, 2, wcslen(ProxyCPPClose_FORMAT), file);
            
    
        }

        fclose(file);
    }
}

#define STUB_H_STARTFORMAT \
    L"\n\
class Stub\n{ \n public:\n\
"
#define STUB_H_DEF_FORMAT L"\t virtual void %s {}\n"
#define STUB_H_CLOSEFORMAT                                             \
    L"\t void PacketProc(ull SessionID, CMessage *msg, WORD " \
    L"byType); \n};\n"

#define STUB_CPP_STARTFORMAT                                              \
    L"\nvoid Stub::PacketProc(ull SessionID, CMessage *msg, WORD " \
    L"byType)\n {\n \
    \tCTestServer *server;\n \
    \t server = static_cast<CTestServer *>(this); \n \
    \n\tswitch(byType)\n \t{\n"
//  int a ;
#define STUB_CPP_OPEN_FORMAT L"\tcase %s :\n \t{ \n"
#define STUB_CPP_DEF_FORMAT L"\t\t %s %s ;\n"
#define STUB_CPP_DEF_FORMAT2 L" >> %s;\n"
#define STUB_CPP_DEF_ARRFORMAT L"\t\t %s %s[%d] ;\n"
#define STUB_CPP_DEF_ARRFORMAT2 L"\t\t msg->GetData(%s,%d);\n"
#define STUB_CPP_DEF_ARRFORMAT3 L"\t\t msg->GetData(%s,%s);\n"

#define STUB_CPP_DEF_ARRBUFFERSIZE 2000

#define STUB_CPP_FUNCTION_FORMAT L"\t\tserver->%s"
#define STUB_CPP_DEFCLOSE_FORMAT L"\t\t break; \n \t}\n"
#define STUB_CPP_CLOSE_FORMAT L"\n \t}\n \n }\n"
void MakeStub()
{
    FILE *file;

    // Stub.h
    {
        wchar_t route[FILENAME_MAX];

        StringCchPrintfW(route, FILENAME_MAX, TargetPath, L"Stub.h");
        _wfopen_s(&file, route, L"w, ccs = UTF-16LE");

        if (file == nullptr)
            __debugbreak();

        wchar_t buffer[BufferMax];

        {
            wchar_t Value[BufferMax];
            wchar_t headerTag[BufferMax];

            Parser parser;
            parser.LoadFile(L"Config.txt");

            // ProxyHeader 작성.
            int Include_StubHeaderCnt;
            parser.GetValue(L"Include_StubHeaderCnt", Include_StubHeaderCnt);
            for (int i = 1; i <= Include_StubHeaderCnt; i++)
            {
                StringCchPrintfW(headerTag, BufferMax, L"Include_StubHeader%d", i);
                parser.GetValue(headerTag, Value, BufferMax);

                StringCchPrintfW(buffer, BufferMax, L"%s\n", Value);
                fwrite(buffer, 2, wcslen(buffer), file);
            }
        }

        fwrite(STUB_H_STARTFORMAT, 2, wcslen(STUB_H_STARTFORMAT), file);

        for (wchar_t *data : gRPCvec)
        {
            std::wstring str(data);
            if (str.find(L"RES") != std::wstring::npos)
                continue;
            StringCchPrintfW(buffer, BufferMax, STUB_H_DEF_FORMAT, data);
            fwrite(buffer, 2, wcslen(buffer), file);
        }
        fwrite(STUB_H_CLOSEFORMAT, 2, wcslen(STUB_H_CLOSEFORMAT), file);
        fclose(file);
    }
    // Stub.cpp
    {
        wchar_t route[FILENAME_MAX];

        StringCchPrintfW(route, FILENAME_MAX, TargetPath, L"Stub.cpp");
        _wfopen_s(&file, route, L"w, ccs = UTF-16LE");

        if (file == nullptr)
            __debugbreak();
        wchar_t buffer[BufferMax];

        // ProxyCpp 작성.
        {
            wchar_t Value[BufferMax];
            wchar_t headerTag[BufferMax];

            Parser parser;
            parser.LoadFile(L"Config.txt");

            int Include_StubCppCnt;
            parser.GetValue(L"Include_StubCppCnt", Include_StubCppCnt);
            for (int i = 1; i <= Include_StubCppCnt; i++)
            {
                StringCchPrintfW(headerTag, BufferMax, L"Include_StubCpp%d", i);
                parser.GetValue(headerTag, Value, BufferMax);

                StringCchPrintfW(buffer, BufferMax, L"%s\n", Value);
                fwrite(buffer, 2, wcslen(buffer), file);
            }
        }

        fwrite(STUB_CPP_STARTFORMAT, 2, wcslen(STUB_CPP_STARTFORMAT), file);

        for (wchar_t *data : gRPCvec_Notrance)
        {
            size_t rmData = wcslen(data);
            memcpy(buffer, data, wcslen(data) * sizeof(wchar_t));
            buffer[rmData] = L'\0';

            size_t oriLen = wcslen(data);
            size_t currentIdx = 0;

            std::vector<wchar_t *> types;
            std::vector<wchar_t *> words;
            words.clear();
            types.clear();
            std::vector<wchar_t *> wordvals;
            size_t i = 0;

            wchar_t *left = buffer, *right = buffer;
            wchar_t *limit = buffer + wcslen(buffer);

            while (1)
            {

                if (buffer[i] == L'=')
                {
                    i++;
                    wchar_t *temp;
                    WordSplit(buffer + i, buffer + i, limit, &temp);
                    wordvals.push_back(temp);

                    break;
                }
                i++;
            }
            i = 0;
            while (1)
            {

                if (buffer[i] == L'=')
                {
                    while (1)
                    {
                        if (buffer[i] == L',' || buffer[i] == L')')
                            break;
                        i++;
                    }
                    buffer[currentIdx++] = L' ';
                }
                buffer[currentIdx++] = buffer[i++];
                if (i >= oriLen)
                    break;
            }
            buffer[currentIdx++] = L'\0';
            limit = buffer + wcslen(buffer);
            bool bChk = false;
            size_t cnt = 0;
            while (1)
            {

                if (right >= limit)
                    break;

                while (bChk == false)
                {
                    if (*left == L'(')
                    {
                        bChk = true;
                        left++;
                        right = left;
                        break;
                    }
                    left++;
                }
                wchar_t *temp;
                left = WordSplit(left, right, limit, &temp);
                right = left;
                if (*temp == L',')
                    continue;
                cnt++;
                if (cnt % 2 == 1)
                    types.push_back(temp);
                if (cnt % 2 == 0)
                    words.push_back(temp);
            }
            wchar_t buffer2[BufferMax];

            std::wstring str(wordvals[0]);
            if (str.find(L"RES") != std::wstring::npos)
                continue;
            StringCchPrintfW(buffer2, BufferMax, STUB_CPP_OPEN_FORMAT,
                             wordvals[0]);
            fwrite(buffer2, 2, wcslen(buffer2), file);

            {
                int cnt = types.size() < words.size() - TypeOffset ? types.size() : words.size() - TypeOffset;
                for (int i = 2; i < cnt; i++)
                {
                    {
                        std::wstring temp(types[i]);

                        if (temp.find(needle1) != std::wstring::npos)
                        {
                            long arraySize = GetSize_ForSizeArray(temp);
                            StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_ARRFORMAT,
                                             L"WCHAR", words[i], arraySize );
                        }
                        else if (temp.find(needle2) != std::wstring::npos)
                        {
                            long arraySize = GetSize_ForSizeArray(temp);
                            StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_ARRFORMAT,
                                             L"WCHAR", words[i], STUB_CPP_DEF_ARRBUFFERSIZE);

                        }
                        else
                        {
                            StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_FORMAT,
                                             types[i], words[i]);
                        }
                    }
          
                    fwrite(buffer2, 2, wcslen(buffer2), file);
                }
            }
            
            i = 2;
            for (std::vector<wchar_t *>::iterator iter = words.begin() + 2;
                 iter != words.end() - TypeOffset; iter++)
            {
                std::wstring temp(types[i]);
                if (temp.find(needle1) != std::wstring::npos)
                {
                    long arraySize = GetSize_ForSizeArray(temp);

                    StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_ARRFORMAT2,
                                     words[i], arraySize * 2);
                }
                else if (temp.find(needle2) != std::wstring::npos)
                {
                    long arraySize = GetSize_ForSizeArray(temp);

                    StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_ARRFORMAT3,
                                     words[i], words[i - 1]);
                }
                else
                {
                    fwrite(L"\t\t *msg", 2, wcslen(L"\t\t *msg"), file);
                    StringCchPrintfW(buffer2, BufferMax, STUB_CPP_DEF_FORMAT2,
                                     *iter);
         
                }
                i++;
                fwrite(buffer2, 2, wcslen(buffer2), file);
            }
          

            right = data;
            while (1)
            {
                if (*right == L'(')
                {
                    right++;
                    break;
                }
                right++;
            }
            memcpy(buffer, data, sizeof(wchar_t) * (right - data));
            buffer[right - data] = L'\0';

            StringCchPrintfW(buffer2, BufferMax, STUB_CPP_FUNCTION_FORMAT, buffer);
            fwrite(buffer2, 2, wcslen(buffer2), file);

            for (int i = 0; i < words.size() - TypeOffset; i++)
            {
                if (i == words.size() - (TypeOffset + 1))
                {
                    StringCchPrintfW(buffer, BufferMax, L"%s );\n", words[i]);
                    fwrite(buffer, 2, wcslen(buffer), file);
                }
                else
                {
                    StringCchPrintfW(buffer, BufferMax, L"%s ,", words[i]);
                    fwrite(buffer, 2, wcslen(buffer), file);
                }
            }
            fwrite(STUB_CPP_DEFCLOSE_FORMAT, 2,
                   wcslen(STUB_CPP_DEFCLOSE_FORMAT), file);
        }
        fwrite(STUB_CPP_CLOSE_FORMAT, 2, wcslen(STUB_CPP_CLOSE_FORMAT), file);
        fclose(file);
    }
}
