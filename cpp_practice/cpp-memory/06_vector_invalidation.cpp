#include <iostream>
#include <vector>

int main() {
    std::vector<int> v;

    v.push_back(10);

    int* p = &v[0];

    std::cout << "before: " << *p << std::endl;
    std::cout << "before address: " << p << std::endl;

    for (int i = 0; i < 100; i++) {
        v.push_back(i);
    }

    std::cout << "after vector data: " << v.data() << std::endl;

    // v가 재할당되었다면 p는 더 이상 유효하지 않음
    std::cout << "after: " << *p << std::endl;

    return 0;
}

// 포인터로 vector를 들여다보는 경우, vector가 재할당되면 포인터가 무효화될 수 있음
// vector는 내부적으로 동적 배열을 사용하기 때문에, 요소가 추가될 때마다 필요한 경우 더 큰 메모리 블록으로 재할당할 수 있음
// 재할당이 발생하면 기존 요소들은 새로운 위치로 복사되고, 이전 메모리 블록은 해제됨

// reverse(100)을 사용하여 vector의 크기를 미리 설정하면 재할당이 발생하지 않도록 할 수 있음