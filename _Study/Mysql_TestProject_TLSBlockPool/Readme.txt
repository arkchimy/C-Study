
구성요소에 해당 경로를 추가하면 <>로 사용가능
//PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;%PATH%

// Mysql_TestProject.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
// 예제 사용법

MYSQL conn;
 MYSQL *connection = NULL;
 MYSQL_RES *sql_result;
 MYSQL_ROW sql_row;
 int query_stat;

// 초기화
 mysql_init(&conn);

// DB 연결

 connection = mysql_real_connect(&conn, "127.0.0.1", "root", "123123", "Test", 3306, (char *)NULL, 0);
 if (connection == NULL)
{
    // mysql_errno(&_MySQL);
    fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
    return 1;
}

// Select 쿼리문
 const char *query = "SELECT * FROM account"; // From 다음 DB에 존재하는 테이블 명으로 수정하세요
 query_stat = mysql_query(connection, query);
 if (query_stat != 0)
{
     printf("Mysql query error : %s", mysql_error(&conn));
     return 1;
 }

// 결과출력
 sql_result = mysql_store_result(connection); // 결과 전체를 미리 가져옴
                                              //	sql_result=mysql_use_result(connection);		// fetch_row 호출시 1개씩 가져옴

 while ((sql_row = mysql_fetch_row(sql_result)) != NULL)
{
     // 실제 컬럼이 int형 타입이었다. 우리가 이거를 문자열에서 다시 숫자로 변환시켜야 돼요
     printf("%2s %2s %s\n", sql_row[0], sql_row[1], sql_row[2]);
 }
 mysql_free_result(sql_result);

// DB 연결닫기
 mysql_close(connection);

//	int a = mysql_insert_id(connection);

//	query_stat = mysql_set_server_option(connection, MYSQL_OPTION_MULTI_STATEMENTS_ON);
//	mysql_next_result(connection);				// 멀티쿼리 사용시 다음 결과 얻기
//	sql_result=mysql_store_result(connection);	// next_result 후 결과 얻음
