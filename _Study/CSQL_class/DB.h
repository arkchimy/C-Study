#pragma once
#define WIN32_LEAN_AND_MEAN

#include "C:\Program Files\MySQL\MySQL Server 8.0\include\mysql.h"
#include <cstdlib> // atoi, atoll
#include <cstring> // _stricmp
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <strsafe.h>

class DB
{
  public:
    DB()
    {
        _conn = mysql_init(nullptr);
        if (!_conn)
            throw std::runtime_error("mysql_init failed");
    }
    ~DB()
    {
        if (_conn)
            mysql_close(_conn);
        _conn = nullptr;
    }

    DB(const DB &) = delete;
    DB &operator=(const DB &) = delete;

    void Connect(const char *host, const char *user, const char *pass,
                 const char *db, unsigned int port = 3306)
    {
        if (!mysql_real_connect(_conn, host, user, pass, db, port, nullptr, 0))
            throw std::runtime_error(std::string("connect fail: ") + mysql_error(_conn));
    }

    // ---------- Value ----------
    class Value
    {
      public:
        Value() = default;
        Value(const char *p, unsigned long len) : _p(p), _len(len) {}

        bool IsNull() const { return _p == nullptr; }

        // NOTE: 엄격 파싱이 필요하면 strtol/strtoll로 에러 체크 권장
        int AsInt(int def = 0) const { return _p ? std::atoi(_p) : def; }
        long long AsInt64(long long def = 0) const { return _p ? std::atoll(_p) : def; }

        std::string AsString() const { return _p ? std::string(_p, _len) : std::string(); }

      private:
        const char *_p = nullptr;
        unsigned long _len = 0;
    };

    // ---------- Row ----------
    class Row
    {
      public:
        Row() = default;

        Row(MYSQL_ROW row, unsigned long *lengths,
            const std::unordered_map<std::string, int> *colIndex)
            : _row(row), _lengths(lengths), _colIndex(colIndex) {}

        // row["Level"]
        Value operator[](const char *colName) const
        {
            if (!_row || !_colIndex)
                return Value{};
            auto it = _colIndex->find(colName);
            if (it == _colIndex->end())
                return Value{}; // 없는 컬럼 -> null 취급(원하면 assert)
            int idx = it->second;
            return Value(_row[idx], _lengths ? _lengths[idx] : 0);
        }

        // row.At(0)
        Value At(int idx) const
        {
            if (!_row || idx < 0)
                return Value{};
            return Value(_row[idx], _lengths ? _lengths[idx] : 0);
        }

        explicit operator bool() const { return _row != nullptr; }

      private:
        MYSQL_ROW _row = nullptr;
        unsigned long *_lengths = nullptr;
        const std::unordered_map<std::string, int> *_colIndex = nullptr;
    };

    // ---------- ResultSet ----------
    class ResultSet
    {
      public:
        ResultSet() = default;
        ~ResultSet()
        {
            if (_res)
                mysql_free_result(_res);
        }

        ResultSet(const ResultSet &) = delete;
        ResultSet &operator=(const ResultSet &) = delete;

        ResultSet(ResultSet &&o) noexcept
        {
            MoveFrom(o);
        }
        ResultSet &operator=(ResultSet &&o) noexcept
        {
            if (this != &o)
            {
                if (_res)
                    mysql_free_result(_res);
                _res = nullptr;
                _colIndex.clear();
                _ok = true;
                _affected = 0;
                _err.clear();
                MoveFrom(o);
            }
            return *this;
        }

        bool Ok() const { return _ok; }
        const std::string &Error() const { return _err; }
        unsigned long long Affected() const { return _affected; }
        bool HasRows() const { return _res != nullptr; }

        // 한 줄씩 fetch
        Row Fetch()
        {
            if (!_res)
                return Row{};
            MYSQL_ROW r = mysql_fetch_row(_res);
            if (!r)
                return Row{};
            unsigned long *lens = mysql_fetch_lengths(_res);
            return Row(r, lens, &_colIndex);
        }

        // range-for 지원(간단 iterator)
        class Iterator
        {
          public:
            Iterator(ResultSet *rs, bool end) : _rs(rs), _end(end)
            {
                if (!_end)
                    ++(*this);
            }
            Row operator*() const { return _cur; }
            Iterator &operator++()
            {
                _cur = _rs->Fetch();
                if (!_cur)
                    _end = true;
                return *this;
            }
            bool operator!=(const Iterator &o) const { return _end != o._end; }

          private:
            ResultSet *_rs = nullptr;
            bool _end = false;
            Row _cur{};
        };

        Iterator begin() { return Iterator(this, false); }
        Iterator end() { return Iterator(this, true); }

      private:
        friend class DB;

        void MoveFrom(ResultSet &o)
        {
            _res = o._res;
            o._res = nullptr;
            _colIndex = std::move(o._colIndex);
            _ok = o._ok;
            _affected = o._affected;
            _err = std::move(o._err);
        }

        void BuildColumnIndex()
        {
            _colIndex.clear();
            if (!_res)
                return;

            unsigned int n = mysql_num_fields(_res);
            MYSQL_FIELD *fields = mysql_fetch_fields(_res);
            for (unsigned int i = 0; i < n; ++i)
            {
                // 컬럼명 그대로 key로 (대소문자 민감 싫으면 normalize해도 됨)
                _colIndex.emplace(fields[i].name, (int)i);
            }
        }

      private:
        MYSQL_RES *_res = nullptr;
        std::unordered_map<std::string, int> _colIndex;

        bool _ok = true;
        unsigned long long _affected = 0;
        std::string _err;
    };

    // 네가 원하는 "Query(string)" 하나
    ResultSet Query(const char *sqlFormat, ...)
    {
        ResultSet out;
        va_list va;
        HRESULT cchRetval;

        char Querybuffer[1000];

        va_start(va, sqlFormat);
        cchRetval = StringCchVPrintfA(Querybuffer, sizeof(Querybuffer) / sizeof(wchar_t), sqlFormat, va);
        va_end(va);

        if (mysql_query(_conn, Querybuffer) != 0)
        {
            out._ok = false;
            out._err = mysql_error(_conn);
            return out;
        }

        out._affected = mysql_affected_rows(_conn);

        MYSQL_RES *res = mysql_store_result(_conn);
        if (!res)
        {
            if (mysql_errno(_conn) != 0)
            {
                out._ok = false;
                out._err = mysql_error(_conn);
            }
            return out; // INSERT/UPDATE면 정상적으로 res==nullptr
        }

        out._res = res;
        out.BuildColumnIndex(); // ⭐ 여기서 컬럼명->인덱스 매핑 완성
        return out;
    }

  private:
    MYSQL *_conn = nullptr;
};

//#pragma once
//
//#include "C:\Program Files\MySQL\MySQL Server 8.0\include\mysql.h"
//#include <unordered_map>
//
//class DB
//{
//  public:
//    DB()
//    {
//    }
//
//    ~DB()
//    {
//    }
//
//    // 결과 RAII
//    class Select_Result
//    {
//      public:
//        Select_Result() = default;
//        Select_Result(MYSQL_RES *r) : _res(r) {}
//        ~Select_Result()
//        {
//            if (_res)
//                mysql_free_result(_res);
//        }
//
//      private:
//        MYSQL_RES *_res = nullptr;
//    };
//    struct QueryResult
//    {
//        // SELECT면 result가 있고, INSERT/UPDATE면 result는 비어있음
//        Select_Result result;
//        unsigned long long affected = 0;
//        unsigned int errno_code = 0;
//
//        bool ok = true;
//        bool hasResultSet = false;
//    };
//
//    // ---------- Value ----------
//    class Value
//    {
//      public:
//        Value() = default;
//        Value(const char *p, unsigned long len) : _p(p), _len(len) {}
//
//        bool IsNull() const { return _p == nullptr; }
//
//        int AsInt(int def = 0) const { return _p ? std::atoi(_p) : def; }
//        long long AsInt64(long long def = 0) const { return _p ? std::atoll(_p) : def; }
//
//        std::string AsString() const { return _p ? std::string(_p, _len) : std::string(); }
//
//      private:
//        const char *_p = nullptr;
//        unsigned long _len = 0;
//    };
//
//     // ---------- Row ----------
//    class Row
//    {
//      public:
//        Row() = default;
//
//        Row(MYSQL_ROW row, unsigned long *lengths,
//            const std::unordered_map<std::string, int> *colIndex)
//            : _row(row), _lengths(lengths), _colIndex(colIndex) {}
//
//        // row["Level"]
//        Value operator[](const char *colName) const
//        {
//            if (!_row || !_colIndex)
//                return Value{};
//            auto it = _colIndex->find(colName);
//            if (it == _colIndex->end())
//                return Value{}; // 없는 컬럼 -> null 취급(원하면 assert)
//            int idx = it->second;
//            return Value(_row[idx], _lengths ? _lengths[idx] : 0);
//        }
//
//        // row.At(0)
//        Value At(int idx) const
//        {
//            if (!_row || idx < 0)
//                return Value{};
//            return Value(_row[idx], _lengths ? _lengths[idx] : 0);
//        }
//
//        explicit operator bool() const { return _row != nullptr; }
//
//      private:
//        MYSQL_ROW _row = nullptr;
//        unsigned long *_lengths = nullptr;
//        const std::unordered_map<std::string, int> *_colIndex = nullptr;
//    };
//
//    void Connect(const char *host, const char *user, const char *pass,
//                 const char *db, unsigned int port = 3306);
//
//
//    QueryResult Query(const char *sql);
//
//
//    DB(const DB &) = delete;
//    DB &operator=(const DB &) = delete;
//
//    MYSQL *connection = NULL;
//};