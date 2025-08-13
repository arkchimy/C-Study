#pragma once
#include <list>
#include <Windows.h>

enum class ESearchType
{
    SucceseSearch = 0,
    HasNotTag,
    MissSemicolon,
    MissEqule,
    HasNotLineBreak,
    StringValue,     // ���ڿ� ������ ����.
    HasNotquotation, //  ����ǥ ����
    BufferOverflow,  // ���� �����÷ο�
    MAX,
};

class ParserManager
{
  public:
    bool static existChk(const WCHAR *_target);
    static std::list<const WCHAR *> _tags;
};

class Parser
{
  public:
    Parser();
    ~Parser();

  public:
    bool LoadFile(const WCHAR *filename);
    bool GetValue(const WCHAR *tag, unsigned short &out);
    bool GetValue(const WCHAR *tag, short &out);
    bool GetValue(const WCHAR *tag, int &out);
    bool GetValue(const WCHAR *tag, bool &out);
    bool GetValue(const WCHAR *tag, WCHAR *out, size_t bufferSize);

  private:
    ESearchType SearchValue(const WCHAR **left, size_t &_len);
    size_t SearchTag(const WCHAR **pleft, const WCHAR *tag);
    WCHAR *buffer = nullptr;

    const WCHAR *logFilename;
    FILE *OpenReadFile = nullptr;
};
