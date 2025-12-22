#pragma once

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include "C:\Program Files\MySQL\MySQL Server 8.0\include\mysql.h"
#include <string>
#include <strsafe.h>
#include <unordered_map>
#pragma comment(lib, "libmysql.lib")
class CDB
{
  public:
    ~CDB()
    {
        if (!_connection)
            return;
        mysql_close(_connection);
    }
    void Connect(const char *host, const char *user, const char *pass,
                 const char *db, unsigned int port = 3306)
    {

        _connection = mysql_init(nullptr);

        _connection = mysql_real_connect(_connection, host, user, pass, db, port, nullptr, 0);
        if (_connection == nullptr)
        {
            printf("Mysql connection error : %s", mysql_error(_connection));
            __debugbreak();
        }
    }

    friend class stRAIIBegin;
    // 실제 값
    class Value
    {
      public:
        Value() = default;
        Value(const char *p, unsigned long len)
            : _p(p), _len(len) {}

        int AsInt() const
        {
            if (_p == nullptr)
                __debugbreak();
            return std::atoi(_p);
        }
        long long AsInt64() const
        {
            if (_p == nullptr)
                __debugbreak();
            return std::atoll(_p);
        }
        std::string AsString() const
        {
            if (_p == nullptr || _len < 0)
            {
                // _p 값이 null인 경우가 존재. DB에서 값을 안넣음.
                if (_len != 0)
                    __debugbreak();
                return std::string();
            }

            return std::string(_p, _len);
        }

      private:
        const char *_p = nullptr;
        unsigned long _len = 0;
    };

    // Fetch의 단위.
    class Row
    {
      public:
        Row() = default;
        Row(MYSQL_ROW row, unsigned long *len, std::unordered_map<std::string, int> *hashMap)
            : _row(row), _len(len), _pHashMap(hashMap) {}

        Value operator[](const char *colName) const
        {
            if (_len == nullptr)
                __debugbreak();

            if (_row == nullptr || _pHashMap == nullptr)
                return Value();

            // 만일 찾지못한다면 Value의 멤버변수 _p 가 Nullptr임.
            auto it = _pHashMap->find(colName);
            if (it == _pHashMap->end())
                return Value();

            int idx = it->second;
            return Value(_row[idx], _len[idx]);
        }
        explicit operator bool() const { return _row != nullptr; }

      private:
        MYSQL_ROW _row = nullptr;
        unsigned long *_len = nullptr;
        //  [ colName ]  = [ Value값이 들어있는 row의 index ]
        std::unordered_map<std::string, int> *_pHashMap = nullptr;
    };

    class ResultSet
    {
      public:
        friend class CDB;
        class Iterator
        {
          public:
            Iterator(ResultSet *pResult, bool end)
                : _pResult(pResult), _end(end)
            {
                if (end == false)
                    ++(*this);
            };
            const Iterator &operator++()
            {
                _current = _pResult->Fetch();
                if (!_current)
                    _end = true;
                return *this;
            }
            bool operator!=(const Iterator &other) const { return _end != other._end; }
            Row operator*() const { return _current; }

          private:
            ResultSet *_pResult;
            bool _end = false;
            Row _current;
        };
        Iterator begin() { return Iterator(this, false); }
        Iterator end() { return Iterator(this, true); }
        ResultSet() = default;
        ~ResultSet()
        {
            if (_pResult != nullptr)
                mysql_free_result(_pResult);
        }
        ResultSet(const ResultSet &) = delete;
        ResultSet &operator=(const ResultSet &) = delete;

        ResultSet (ResultSet&& other) noexcept
            : _pResult(other._pResult),
              _bSuccess(other._bSuccess),
              _err(std::move(other._err)),
              _HashMap(std::move(other._HashMap))
        {
            other._pResult = nullptr;
            other._bSuccess = false;
        }

        inline bool Sucess() const { return _bSuccess; }
        const std::string &Error() const { return _err; }

        void InitHashTable()
        {
            _HashMap.clear();
            if (_pResult == nullptr)
                return;
            MYSQL_FIELD *fields = mysql_fetch_fields(_pResult);
            unsigned int n = mysql_num_fields(_pResult);
            for (unsigned int i = 0; i < n; i++)
            {
                _HashMap.emplace(fields[i].name, i);
            }
        }

        Row Fetch()
        {
            // Query 내부에서 InitHashTable를 호출 함.
            if (_pResult == nullptr)
                return Row();
            MYSQL_ROW row = mysql_fetch_row(_pResult);
            if (row == nullptr)
                return Row();
            // 원소마다의 길이를 반환
            unsigned long *plens = mysql_fetch_lengths(_pResult);
            return Row(row, plens, &_HashMap);
        }

      private:
        std::unordered_map<std::string, int> _HashMap;
        bool _bSuccess = true;
        MYSQL_RES *_pResult = nullptr;
        std::string _err;
    };

    ResultSet Query(const char *sqlFormat, ...)
    {
        ResultSet out;
        va_list va;
        HRESULT cchRetval;

        char Querybuffer[1000];

        va_start(va, sqlFormat);
        cchRetval = StringCchVPrintfA(Querybuffer, sizeof(Querybuffer) / sizeof(wchar_t), sqlFormat, va);
        va_end(va);

        if (mysql_query(_connection, Querybuffer) != 0)
        {
            out._bSuccess = false;
            out._err = mysql_error(_connection);
            return out;
        }

        MYSQL_RES *res = mysql_store_result(_connection);
        if (!res)
        {
            if (mysql_errno(_connection) != 0)
            {
                out._bSuccess = false;
                out._err = mysql_error(_connection);
            }
            return out; // INSERT/UPDATE면 정상적으로 res==nullptr
        }

        out._pResult = res;
        out.InitHashTable(); // ⭐ 여기서 컬럼명->인덱스 매핑 완성
        return out;
    }

  private:
    MYSQL *_connection;
};

struct stRAIIBegin
{

    stRAIIBegin(CDB *db)
        : _connection(db->_connection)
    {
        if (_connection == nullptr)
        {
            __debugbreak();
            return;
        }
        if (mysql_query(_connection, "BEGIN") != 0)
        {
            printf("START TRANSACTION error: %s\n", mysql_error(_connection));
            __debugbreak();
        }
    }
    ~stRAIIBegin()
    {
        if (mysql_query(_connection, "Commit") != 0)
        {
            printf("START TRANSACTION error: %s\n", mysql_error(_connection));
            __debugbreak();
        }
    }

    MYSQL *_connection = nullptr;
};