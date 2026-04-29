#include <iostream>
#include <vector>

// void leak() {
//     int* p = new int[100];

//     p[0] = 42;

//     std::cout << p[0] << std::endl;

//     // delete[] p; 없음
// }

void leak() {
    std::vector<int> p(100);

    p[0] = 42;

    std::cout << p[0] << std::endl;
}

int main() {
    leak();
    return 0;
}