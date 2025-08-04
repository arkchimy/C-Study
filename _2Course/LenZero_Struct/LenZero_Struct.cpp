
#include <Windows.h>
#include <iostream>

struct st_lenZero
{
    DWORD len;
    char ch[0];
};

int main()
{
    st_lenZero *header;
    // 특정한 로직에 의해 Len이 4로 결정!

    header = (st_lenZero *)malloc(sizeof(st_lenZero) + 4);
    if (header == nullptr)
        __debugbreak();

    header->ch[0] = 'a';
    header->ch[1] = 'b';
    header->ch[2] = 'c';
    header->ch[3] = 0;

    printf("%s", header->ch);
}
