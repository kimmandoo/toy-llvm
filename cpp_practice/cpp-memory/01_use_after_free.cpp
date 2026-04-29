#include <iostream>

int main() {
    int* p = new int(42);

    std::cout << "before delete: " << *p << std::endl;

    // delete p;
    // // 이미 해제한 메모리를 다시 읽음
    // std::cout << "after delete: " << *p << std::endl;

    delete p;
    p = nullptr;

    if (p != nullptr) {
        std::cout << "after delete: " << *p << std::endl;
    }

    return 0;
}