#include <iostream>
#include <vector>

int main() {
    // int* arr = new int[3];

    // arr[0] = 10;
    // arr[1] = 20;
    // arr[2] = 30;

    // // 범위 밖 접근
    // arr[3] = 40;

    // std::cout << arr[3] << std::endl;

    // delete[] arr;

    std::vector<int> arr(3);

    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;

    std::cout << arr.at(3) << std::endl;

    return 0;
}