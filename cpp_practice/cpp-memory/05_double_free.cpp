#include <iostream>

int main() {
    int* p = new int(10);

    delete p;

    // 같은 주소를 다시 해제
    // delete p;

    return 0;
}