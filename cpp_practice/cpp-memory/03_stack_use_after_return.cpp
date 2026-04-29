#include <iostream>
#include <memory> // 스마트포인터용

int* badFunction() {
    int local = 123;
    return &local;
}

int main() {
    int* p = badFunction();

    std::cout << *p << std::endl;

    return 0;
}

// std::unique_ptr<int> goodFunction() {
//     return std::make_unique<int>(123);
// }

// int main() {
//     auto p = goodFunction();

//     std::cout << *p << std::endl;

//     return 0;
// }