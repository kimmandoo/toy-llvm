#include <iostream>

void foo() {
    int a = 10;
    int b = 20;

    std::cout << &a << "\n";
    std::cout << &b << "\n";
}

int main() {
    foo();
}