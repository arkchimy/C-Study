#pragma once
#include <iostream>

template <typename T>
struct TemplateClass
{
    void foo();
    T data;
};

template <typename T>
void TemplateClass<T>::foo()
{
    std::cout << "FOO\n";
}

