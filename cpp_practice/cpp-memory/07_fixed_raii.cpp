#include <iostream>

class Buffer {
public:
    int* data;

    Buffer() {
        data = new int[100];
        std::cout << "Buffer allocated\n";
    }

    ~Buffer() {
        delete[] data;
        std::cout << "Buffer freed\n";
    }
};

int main() {
    Buffer b1;
    Buffer b2 = b1;

    return 0;
}