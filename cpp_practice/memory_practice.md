프로세스 메모리 구조 → 객체 수명 → 포인터/참조 → RAII → 스마트 포인터 → move → STL/성능 → 디버깅 순서로 학습 예정

```text
프로세스 메모리 구조

+----------------------+
| Code/Text            |  실행 코드
+----------------------+
| Data                 |  초기화된 전역/static 변수
+----------------------+
| BSS                  |  초기화 안 된 전역/static 변수
+----------------------+
| Heap                 |  동적 할당 영역: new, malloc
|                      |  직접 해제하거나 스마트 포인터가 관리
+----------------------+
| Stack                |  지역 변수, 함수 호출 정보
+----------------------+
```

지역변수는 스코프 벗어나면 자동 메모리 해제, heap에있는 건 직접해제 해줘야하고, static이거나 전역에 선언된 변수들은 프로그램 종료 시까지 살아있고, 코드 그 자체도 프로그램 실행 동안 존재한다.

## stack

```cpp
void foo() {
    int a = 10;
    int b = 20;
}
```

foo()가 호출되면 스택에 a,b가 잡히고, foo 가 끝나면 자동으로 사라진다.

## heap

```cpp
int* p = new int(10);

std::cout << *p << "\n";

delete p;
```

heap은 직접 요청해서 쓰는 메모리. 포인터 변수랑 포인터가 가리키는 대상은 다른 메모리기 때문에 p자체는 stack에 위치하는데, p가 가리키는 int는 heap에 위치한다.


포인터는 주소를 저장하는 변수로, 

```cpp
int x = 10;
int* p = &x;
```

&x는 x의 주소, *p는 p가 가리키는 곳의 실제 값이다. 그래서 `cout << p` 로 하면 주소값이 출력 되고, `*p`로 하면 해당 주소에 있는 값이 출력된다.

```cpp
int* foo() {
    int x = 10;
    return &x;
}

int* p = foo();
std::cout << *p << "\n";
```

x는 foo의 지역 변수기 때문에, foo 호출 이후에는 x가 사라진다. 근데 사라진 변수의 주소를 밖으로 반환한건데 이러면 *p는 유효하지않은 객체의 주소를 들고있게 된다. 이걸 dangling pointer라고 한다.

heap에는 내가 직접 변수를 할당해서 쓰는데, gc가 없기 때문에 메모리 누수가 안나도록 사용이 끝난 객체를 삭제해줘야된다.

```cpp
void foo() {
    int* p = new int(10);
}
```
여기서 foo가 끝나면 p는 사라지는데, new int(10)으로 만든 heap 객체는 메모리에 남아있다. 이 상황이 memory leak이다.

foo 함수 마지막에 `delete p;`를 해주면 메모리에서 객체를 삭제한다.

이런 궁금증이 생길 수 있다. 이미 삭제한 객체를 또 삭제하면?

당연히 비정상적인 행위다.

그래서 관례는 delete 후 p에다가 nullptr로 비워주는 걸 하는데 이거도 개발자의 실수로 누수가 날 수 있는 구조이기 때문에 직접 delete하는 걸 피해야된다.

## RAII - Resource Acquisition Is Initialization

리소스 소유권을 객체한테 주고, 객체가 죽을 때 리소스도 정리하는 방식이다.

```cpp
class File {
public:
    File() {
        std::cout << "파일 열기\n";
    }

    ~File() {
        std::cout << "파일 닫기\n";
    }
};

void foo() {
    File f;
}
```

foo에 진입하면 생성자가 호출될 것이고, foo가 종료되면 객체 소멸자가 호출되는 패턴이다.

객체가 스코프를 벗어나면 소멸자가 자동 호출되기 때문에 이런 방식을 권장하는 것으로 보인다.

```cpp
class Person {
public:
    Person() {
        std::cout << "생성\n";
    }

    ~Person() {
        std::cout << "소멸\n";
    }
};

void foo() {
    Person p1;
    Person p2;
}
```

java랑 다르게 선언하면 생성자가 호출된다. 근데 생성시엔 p1부터 생성되는데, 소멸은 p2부터 소멸된다.

나중에 만들어진 객체가 이전에 생성된 객체를 참조할 수 있어서 생성 역순으로 소멸되는 게 안전하다.

### 참조랑 포인터는 무슨 차이

```cpp
int x = 10;

int* p = &x;
int& r = x;
```

참조는 기존 객체의 별명이고, 포인터는 주소를 저장하는 것으로 이해하면 된다.

```cpp
int x = 10;
int y = 20;

int* p = &x;
p = &y;

int& r = x;
r = y;
```

p의 경우에는 x를 참조하고 있다가 y로 참조 대상을 바꾸는 건데, r은 x에 y를 대입하는 것이다..

이 참조 개념은 함수 인자전달 방식에도 적용되니까 매우 중요하다.

```cpp
void foo(int x) {
    x = 20;
}

int a = 10;
foo(a);

std::cout << a << "\n"; // 10
```

foo에는 a의 복사본이 들어간다. 그래서 foo안에서 x에 뭔짓을 해도 a가 변하지 않는다.

근데 포인터를 전달한다면?

```cpp
void foo(int* p) {
    *p = 20;
}

int a = 10;
foo(&a);

std::cout << a << "\n"; // 20
```

주소를 넘긴 것이기 때문에 원본을 바꿀 수 있다.

```cpp
void foo(int& x) {
    x = 20;
}

int a = 10;
foo(a);

std::cout << a << "\n"; // 20
```

참조도 마찬가지로 원본을 바꿀 수 있다.

참조를 사용하면 값을 넘긴 게 아니라 원본에 대한 접근 권한을 준거다.

그래서

```cpp
void print(const std::string& s) {
    std::cout << s << "\n";
}
```

이렇게 쓰면 원본을 const로 받아와서 읽기전용으로 쓰게 하는 것과 같아진다. 복사가 일어나지않기 때문에 조금 빠르다.

cpp에서 배열은 포인터인데, 범위검사를 해주진않는다. 그래서 가능하면 배열 접근 말고 stl의 멤버 함수들을 사용해서 접근해주면 예외처리가 가능하다.

참고로 배열을 new[] 로 생성했으면 delete[]를 써야된다.

포인터로 받아서 delete로 하면 컴파일러가 잡아내지 못하는 에러를 발생시킬 수 있다.

왜?

new[]를 호출하면 시스템은 배열의 크기를 별도의 공간에 저장해 둔다. delete[] 라면 이 정보를 꺼내서 정의된 공간 전체를 해제하지만 delete로 하면 첫 원소 공간만 해제하거나 이상한 주소를 참조할 수 있게 된다.

## 스마트 포인터

cpp의 스마트 포인터는 대표적으로 `unique_ptr`, `shared_ptr`, `weak_ptr` 이렇게 3개가 있다.

`unique_ptr`는 이름 그대로 유일한 소유다.

```cpp
#include <memory>

std::unique_ptr<int> p = std::make_unique<int>(10);

std::cout << *p << "\n";
```

 소유자가 둘 이상이면 누가 해제할 지 애매하기 때문에 복사가 안된다.

그대신 소유권을 넘겨주는 move는 가능하다.

```cpp
auto p1 = std::make_unique<int>(10);
auto p2 = std::move(p1);
```

넘겨 주고 나서는 기존 포인터가 nullptr을 가리키게 된다.

heap객체를 하나의 객체가 명확하게 소유해야한다면 unique_ptr을 사용한다.

`shared_ptr`는 여러 객체가 하나의 리소스를 공유해야 할 때 사용한다.

```cpp
auto p1 = std::make_shared<int>(10);
auto p2 = p1;
auto p3 = p1;
```

p1, p2, p3가 동일한 객체를 보고있고, 내부적으로 참조 카운터가 증가되어있다. 세 변수가 모두 사라지면 그 때서야 heap 객체가 해제된다.

```cpp
{
    auto p1 = std::make_shared<int>(10);
    {
        auto p2 = p1;
    } // p2 사라짐, count 감소
}
```

예시를 위한 극단적인 코드다. 스코프가 끝나니까 p2는 바로 사라지고, p1의 count는 1로 줄어든다. 그리고 바깥 스코프도 끝나면 heap 객체가 해제된다.

근데 이게 문제가 있다. 

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;
};
```

shared_ptr끼리 물고 있으면 참조 카운터가 영원히 0이 될 수 없게 된다. 일종의 상호 점유가 발생하게 되는 것이다.

이때는 weak_ptr이 필요하다. weak_ptr은 참조는 하지만 소유하지는 않는다. 소유권이 없다는 측면에서 raw_ptr랑 비슷하게 생각할 수 있는데, weak_ptr는 대상의 생사 여부를 파악할 수 있다는 점이 다르다.

그리고 *나 -> 로 직접 접근할 수 없고 lock()을 써서 접근해야된다. weak_ptr이 자신의 제어블록을 읽어 객체 생존여부를 본다음 값을 넘겨 주기 때문에 lock을 쓰지 않으면 에러가 난다.

## [Rule of 3,5,0](https://en.cppreference.com/cpp/language/rule_of_three)

cpp에서 복사는 raw pointer가 멤버일 때를 조심해야된다.

```cpp
class Buffer {
public:
    int* data;

    Buffer() {
        data = new int[100];
    }

    ~Buffer() {
        delete[] data;
    }
};
```

객체 복사를 진행하면 멤버 별 복사가 일어나는데, 기본 복사는 포인터 값만 복사한다. 그래서 같은 heap메모리를 가리키가 되기에 원복 객체, 복사한 객체가 모두 소멸할 시점에 double delete가 발생하는 문제가 있다.

그래서 raw pointer가 멤버면 생성자, 소멸자를 제대로 구현해놔야된다..

### Rule of Three

```cpp
class Buffer {
public:
    int* data;

    Buffer() {
        data = new int[100];
    }

    ~Buffer() {
        delete[] data;
    }

    Buffer(const Buffer& other) {
        data = new int[100];
        std::copy(other.data, other.data + 100, data);
    }

    Buffer& operator=(const Buffer& other) {
        if (this == &other) return *this;

        delete[] data;
        data = new int[100];
        std::copy(other.data, other.data + 100, data);

        return *this;
    }
};
```

리소스를 직접 관리하는 클래스가 있으면 3개를 생각해야된다. 소멸자, 복사 생성자, 복사 대입 생성자.

포인터 문제 같은 게 발생하지 않도록 새 객체 생성, 기존 객체 대입 시 깊은 복사를 직접 처리해주는 작업이다.

근데 vector는 이게 되어있어서 포인터 쓰지말고 그냥 이렇게 쓰면 된다.

```cpp
class Buffer {
public:
    std::vector<int> data;

    Buffer() : data(100) {}
};
```

이러면 복사, 이동, 소멸을 vector에서 알아서 처리한다.

### Rule of Five

```cpp
class Buffer {
public:
    int* data;
    size_t size;

    Buffer(size_t size)
        : data(new int[size]), size(size) {}

    ~Buffer() {
        delete[] data;
    }

    Buffer(const Buffer& other)
        : data(new int[other.size]), size(other.size) {
        std::copy(other.data, other.data + size, data);
    }

    Buffer& operator=(const Buffer& other) {
        if (this == &other) return *this;

        int* newData = new int[other.size];
        std::copy(other.data, other.data + other.size, newData);

        delete[] data;

        data = newData;
        size = other.size;

        return *this;
    }

    Buffer(Buffer&& other) noexcept
        : data(other.data), size(other.size) {
        other.data = nullptr;
        other.size = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        if (this == &other) return *this;

        delete[] data;

        data = other.data;
        size = other.size;

        other.data = nullptr;
        other.size = 0;

        return *this;
    }
};
```

기존 3개에서 move가 추가된 것이다. 복사는 깊은 복사로, 이동은 소유권만 가져온다.

```cpp
std::string a = "hello";
std::string b = std::move(a);
```

move는 먼저 a를 이동해도 되는 값으로 캐스팅한다. move 자체는 메모리를 옮기지않고, 이동 생성자나 이동 대입 연산자가 호출될 수 있게 캐스팅해준다.

## const

`const`는 해당 값을 더이상 수정할 수 없게 만드는 키워드인데 이게 포인터랑 합쳐지면 헷갈린다.

`const int* p;`는 p가 가리키는 int를 수정할 수 없다는 뜻이기 때문에, p 자체에 새 주소를 배정하는 건 가능하다..

```cpp
int a = 10;
int b = 20;

const int* p = &a;

*p = 30; // 불가
p = &b;  // 가능
```

근데 `int* const p = &a;` 이건 p자체가 다른 주소를 가리킬 수 없다는 뜻이다.

```cpp
int* const p = &a;

*p = 30; // 가능
p = &b;  // 불가
```

현재 가리키는 주소의 대상값을 바꿀 수는 있지만 주소를 바꿀 순 없다.

그럼, 어처구니 없지만 `const int* const p = &a;`로 하면?

새 대상 값을 넣는 것도, 새 주소 값으로 배정하는 것도 안된다.

const 위치가 매우 중요하다. 그래서 포인터를 읽을 때 `int *p` 라고 읽지 말고 `int* p`라고 읽어야 헷갈리지 않는다.

한줄에 여러 변수를 선언할 때만 주의하면 포인터를 타입으로 생각하는게 명료하다.

# 현업에서도 그럼 스마트포인터를 쓰는지?

gemini research에 의하면 스마트 포인터 보다는 vector가 연속공간 메모리 할당이라 캐시 효율이 좋기 때문에 vector를 활용한다고함.. 

스마트 포인터도 결국 내부적으로는 힙 할당을 수행하니 new, delete가 일어난다고 할 수 있어서 미리 큰 메모리 풀을 잡아두고 그 안에서 객체 할당/해제를 하기도 하나 봄.

성능을 올린다 -> 힙 할당 덜 쓰고 캐시 hit 높인다

# 디버깅 실습

메모리 디버깅 실습을 해보겠다.

실습 환경은 windows + wsl(ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential clang gdb valgrind cmake ninja-build
```

도구들 설치. 우분투니까 g++을 써보자. 

`g++ -g -O0 01_use_after_free.cpp -o app` 이렇게 기본 빌드한다.

-g는 코드라인 같은 디버깅 심볼을 포함하는 옵션

AddressSanitizer빌드는 `clang++ -g -O1 -fsanitize=address -fno-omit-frame-pointer 01_use_after_free.cpp -o app_asan` 이렇게.

-fsanitize 활성화를하고, fno-omit으로 stack trace 구체화를 시킴. ASan에서는 -O1이 권장 최적화 옵션이라고 함.

### 01

```cpp
#include <iostream>

int main() {
    int* p = new int(42);

    std::cout << "before delete: " << *p << std::endl;

    delete p;

    // 이미 해제한 메모리를 다시 읽음
    std::cout << "after delete: " << *p << std::endl;

    return 0;
}
```

해제한 메모리를 읽는 건데, after에서 실행은 되는데 쓰레기 값이 들어가있다.

이걸 AddressSanitizer로 실행하면? 

```bash
before delete: 42
=================================================================
==171389==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 at pc 0x55b2e93fee8d bp 0x7ffe339708b0 sp 0x7ffe339708a8
READ of size 4 at 0x502000000010 thread T0
    #0 0x55b2e93fee8c in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/01_use_after_free.cpp:11:38
    #1 0x7ffb252e91c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #2 0x7ffb252e928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #3 0x55b2e93233a4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf_asan+0x2c3a4) (BuildId: 498dfdb57e7e07435e35b0cb1082f99d3dc808c8)

0x502000000010 is located 0 bytes inside of 4-byte region [0x502000000010,0x502000000014)
freed by thread T0 here:
    #0 0x55b2e93fd091 in operator delete(void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf_asan+0x106091) (BuildId: 498dfdb57e7e07435e35b0cb1082f99d3dc808c8)
    #1 0x55b2e93fecfa in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/01_use_after_free.cpp:8:5
    #2 0x7ffb252e91c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7ffb252e928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55b2e93233a4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf_asan+0x2c3a4) (BuildId: 498dfdb57e7e07435e35b0cb1082f99d3dc808c8)

previously allocated by thread T0 here:
    #0 0x55b2e93fc811 in operator new(unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf_asan+0x105811) (BuildId: 498dfdb57e7e07435e35b0cb1082f99d3dc808c8)
    #1 0x55b2e93feba4 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/01_use_after_free.cpp:4:14
    #2 0x7ffb252e91c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7ffb252e928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55b2e93233a4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf_asan+0x2c3a4) (BuildId: 498dfdb57e7e07435e35b0cb1082f99d3dc808c8)

SUMMARY: AddressSanitizer: heap-use-after-free /home/mgkim/toy-llvm/cpp_practice/cpp-memory/01_use_after_free.cpp:11:38 in main
Shadow bytes around the buggy address:
  0x501ffffffd80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x502000000000: fa fa[fd]fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000080: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000100: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000180: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000200: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000280: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==171389==ABORTING
```

ERROR에 문제 종류를 자세히 알려줌.

READ of size 4 at 0x502000000010 thread T0

는 int가 4바이트라 발생한거고,

#0 0x55b2e93fee8c in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/01_use_after_free.cpp:11:38

이건 발생한 위치 알려주는 것.

무슨 버그? -> 어디서 잘못 접근? -> 어디서 해제? -> 어디서 할당?

을 체크한다.

delete하고 나서 nullptr 넣어주고, 방어코드 넣는 거로 해결 가능.

```cpp
delete p;
p = nullptr;

if (p != nullptr) {
    std::cout << "after delete: " << *p << std::endl;
}
``

### 02
버퍼오버플로우인데 그냥 배열 범위 밖에서 접근이랑 동일

`clang++ -g -O0 -fsanitize=address -fno-omit-frame-pointer 02_heap_overflow.cpp -o overflow_asan
./overflow_asan`

```bash
=================================================================
==173996==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x50200000001c at pc 0x55aebe588c8b bp 0x7ffca23ab210 sp 0x7ffca23ab208
WRITE of size 4 at 0x50200000001c thread T0
    #0 0x55aebe588c8a in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/02_heap_overflow.cpp:11:12
    #1 0x7f273fa211c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #2 0x7f273fa2128a in __libc_start_main csu/../csu/libc-start.c:360:3
    #3 0x55aebe4ad364 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/overflow_asan+0x2c364) (BuildId: 99df0626185cee03ea2f2327b6b43d130aa06c30)

0x50200000001c is located 0 bytes after 12-byte region [0x502000000010,0x50200000001c)
allocated by thread T0 here:
    #0 0x55aebe5868f1 in operator new[](unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/overflow_asan+0x1058f1) (BuildId: 99df0626185cee03ea2f2327b6b43d130aa06c30)
    #1 0x55aebe588b68 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/02_heap_overflow.cpp:4:16
    #2 0x7f273fa211c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7f273fa2128a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55aebe4ad364 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/overflow_asan+0x2c364) (BuildId: 99df0626185cee03ea2f2327b6b43d130aa06c30)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/mgkim/toy-llvm/cpp_practice/cpp-memory/02_heap_overflow.cpp:11:12 in main
Shadow bytes around the buggy address:
  0x501ffffffd80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x502000000000: fa fa 00[04]fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000080: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000100: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000180: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000200: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000280: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==173996==ABORTING
```

컴파일러 최적화 -O1이 에러를 먹어버려서 -O0으로 컴파일 했다.

실제 메모리 접근이 일어날 때 검사하는데 최적화 과정에서 메모리 접근이 사라졌나??

어쨌든 고치려면 범위 검사를 해주면 됨.

`vector::at()`로 그 인덱스가 있는 지 확인하고 없으면 std::out_of_range가 발생

```bash
terminate called after throwing an instance of 'std::out_of_range'
  what():  vector::_M_range_check: __n (which is 3) >= this->size() (which is 3)
Aborted (core dumped)
```

### 03
이번꺼는 지역변수 주소 반환 문제다. 

dangling pointer 문제.

```bash
03_stack_use_after_return.cpp:5:13: warning: address of stack memory associated with local variable 'local' returned [-Wreturn-stack-address]
    5 |     return &local;
      |         
```

error는 아니고 warning이 나옴.

local은 함수의 지역변수 -> stack 공간에 위치하는데 함수 끝나면 사라지기 때문에 죽은 주소를 p한테 준다.

### 04

이번엔 메모리 leak.

delete[]를 깜빡한 경우다. 변수 p는 사라졌지만 힙 메모리에 크기 100짜리는 살아있다.

`clang++ -g -O0 -fsanitize=address -fno-omit-frame-pointer 04_memory_leak.cpp -o leak_asan
ASAN_OPTIONS=detect_leaks=1 ./leak_asan`

-O1로 하면 warning이나 그냥 무시되는 경우가 꽤 있다..

그래서 ASAN_OPTIONS에 값을 true로 바꿔놔도 검출이 안되길래 -O0으로 컴파일 중

```bash
=================================================================
==178444==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 400 byte(s) in 1 object(s) allocated from:
    #0 0x55bcd30298f1 in operator new[](unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/leak_asan+0x1058f1) (BuildId: 5dc4bbc99b938c44c355cff3c2d7d72354ce3e32)
    #1 0x55bcd302bb61 in leak() /home/mgkim/toy-llvm/cpp_practice/cpp-memory/04_memory_leak.cpp:4:14
    #2 0x55bcd302bc33 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/04_memory_leak.cpp:14:5
    #3 0x7f87ed2a21c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #4 0x7f87ed2a228a in __libc_start_main csu/../csu/libc-start.c:360:3
    #5 0x55bcd2f50364 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/leak_asan+0x2c364) (BuildId: 5dc4bbc99b938c44c355cff3c2d7d72354ce3e32)

SUMMARY: AddressSanitizer: 400 byte(s) leaked in 1 allocation(s).
```

400byte가 누수된다는 걸 알았다. valgrind로 한번 더 확인해보자.

`g++ -g -O0 04_memory_leak.cpp -o leak` 빌드하고,

`valgrind --leak-check=full --show-leak-kinds=all ./leak` 실행파일을 valgrind leak check로 돌리기

```bash
==179050== Memcheck, a memory error detector
==179050== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==179050== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==179050== Command: ./leak
==179050== 
42
==179050== 
==179050== HEAP SUMMARY:
==179050==     in use at exit: 400 bytes in 1 blocks
==179050==   total heap usage: 3 allocs, 2 frees, 75,152 bytes allocated
==179050== 
==179050== 400 bytes in 1 blocks are definitely lost in loss record 1 of 1
==179050==    at 0x48485C3: operator new[](unsigned long) (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==179050==    by 0x10919E: leak() (04_memory_leak.cpp:4)
==179050==    by 0x1091E5: main (04_memory_leak.cpp:14)
==179050== 
==179050== LEAK SUMMARY:
==179050==    definitely lost: 400 bytes in 1 blocks
==179050==    indirectly lost: 0 bytes in 0 blocks
==179050==      possibly lost: 0 bytes in 0 blocks
==179050==    still reachable: 0 bytes in 0 blocks
==179050==         suppressed: 0 bytes in 0 blocks
==179050== 
==179050== For lists of detected and suppressed errors, rerun with: -s
==179050== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
```

heap summary, leak summary의 definitely lost를 보면 진짜 누수임을 알 수 있음


함수 마지막에 delete[]를 써도 되겠지만 vector를 써보자.

```cpp
void noLeak() {
    std::vector<int> p(100);

    p[0] = 42;

    std::cout << p[0] << std::endl;
}
```

다시 valgrind 돌리면 깨끗하다.

```cpp
==179633== Memcheck, a memory error detector
==179633== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==179633== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==179633== Command: ./leak_@
==179633== 
42
==179633== 
==179633== HEAP SUMMARY:
==179633==     in use at exit: 0 bytes in 0 blocks
==179633==   total heap usage: 3 allocs, 3 frees, 75,152 bytes allocated
==179633== 
==179633== All heap blocks were freed -- no leaks are possible
==179633== 
==179633== For lists of detected and suppressed errors, rerun with: -s
==179633== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 05

double free 문제다.

```bash
=================================================================
==180164==ERROR: AddressSanitizer: attempting double-free on 0x502000000010 in thread T0:
    #0 0x55af672da031 in operator delete(void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x106031) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)
    #1 0x55af672dbbc5 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/05_double_free.cpp:9:5
    #2 0x7fe07f1991c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7fe07f19928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55af67200344 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x2c344) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)

0x502000000010 is located 0 bytes inside of 4-byte region [0x502000000010,0x502000000014)
freed by thread T0 here:
    #0 0x55af672da031 in operator delete(void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x106031) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)
    #1 0x55af672dbbaa in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/05_double_free.cpp:6:5
    #2 0x7fe07f1991c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7fe07f19928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55af67200344 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x2c344) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)

previously allocated by thread T0 here:
    #0 0x55af672d97b1 in operator new(unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x1057b1) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)
    #1 0x55af672dbb48 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/05_double_free.cpp:4:14
    #2 0x7fe07f1991c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #3 0x7fe07f19928a in __libc_start_main csu/../csu/libc-start.c:360:3
    #4 0x55af67200344 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x2c344) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008)

SUMMARY: AddressSanitizer: double-free (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/double_free_asan+0x106031) (BuildId: dd38c7d20bca57bdeeac86e2c06fbfb79b27a008) in operator delete(void*)
==180164==ABORTING
```

ASAN이 double-free라고 알려준다. 

Error가 발생한 부분이 두번쨰 delete 위치고, freed by가 정상 delete 위치.

그냥 벡터쓰면 된다.

### 06

```bash
before: 10
before address: 0x502000000010
after vector data: 0x515000000080
=================================================================
==181271==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 at pc 0x564a2aff12c8 bp 0x7ffe76f4dfc0 sp 0x7ffe76f4dfb8
READ of size 4 at 0x502000000010 thread T0
    #0 0x564a2aff12c7 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/06_vector_invalidation.cpp:21:31
    #1 0x7f62191111c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #2 0x7f621911128a in __libc_start_main csu/../csu/libc-start.c:360:3
    #3 0x564a2af153d4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/vector+0x2c3d4) (BuildId: c064053e32f2423e7b379105e23b2a12df46daa0)

0x502000000010 is located 0 bytes inside of 4-byte region [0x502000000010,0x502000000014)
freed by thread T0 here:
    #0 0x564a2afef0c1 in operator delete(void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/vector+0x1060c1) (BuildId: c064053e32f2423e7b379105e23b2a12df46daa0)
    #1 0x564a2aff0fa0 in std::__new_allocator<int>::deallocate(int*, unsigned long) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/new_allocator.h:172:2
    #2 0x564a2aff0fa0 in std::allocator_traits<std::allocator<int>>::deallocate(std::allocator<int>&, int*, unsigned long) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/alloc_traits.h:517:13
    #3 0x564a2aff0fa0 in std::_Vector_base<int, std::allocator<int>>::_M_deallocate(int*, unsigned long) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/stl_vector.h:390:4
    #4 0x564a2aff0fa0 in void std::vector<int, std::allocator<int>>::_M_realloc_insert<int const&>(__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int>>>, int const&) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/vector.tcc:519:7
    #5 0x564a2aff0fa0 in std::vector<int, std::allocator<int>>::push_back(int const&) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/stl_vector.h:1292:4
    #6 0x564a2aff0fa0 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/06_vector_invalidation.cpp:15:11
    #7 0x7f62191111c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #8 0x7f621911128a in __libc_start_main csu/../csu/libc-start.c:360:3
    #9 0x564a2af153d4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/vector+0x2c3d4) (BuildId: c064053e32f2423e7b379105e23b2a12df46daa0)

previously allocated by thread T0 here:
    #0 0x564a2afee841 in operator new(unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/vector+0x105841) (BuildId: c064053e32f2423e7b379105e23b2a12df46daa0)
    #1 0x564a2aff0bda in std::__new_allocator<int>::allocate(unsigned long, void const*) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/new_allocator.h:151:27
    #2 0x564a2aff0bda in std::allocator_traits<std::allocator<int>>::allocate(std::allocator<int>&, unsigned long) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/alloc_traits.h:482:20
    #3 0x564a2aff0bda in std::_Vector_base<int, std::allocator<int>>::_M_allocate(unsigned long) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/stl_vector.h:381:20
    #4 0x564a2aff0bda in void std::vector<int, std::allocator<int>>::_M_realloc_insert<int>(__gnu_cxx::__normal_iterator<int*, std::vector<int, std::allocator<int>>>, int&&) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/vector.tcc:459:33
    #5 0x564a2aff0bda in int& std::vector<int, std::allocator<int>>::emplace_back<int>(int&&) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/vector.tcc:123:4
    #6 0x564a2aff0bda in std::vector<int, std::allocator<int>>::push_back(int&&) /usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../include/c++/13/bits/stl_vector.h:1299:9
    #7 0x564a2aff0bda in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/06_vector_invalidation.cpp:7:7
    #8 0x7f62191111c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #9 0x7f621911128a in __libc_start_main csu/../csu/libc-start.c:360:3
    #10 0x564a2af153d4 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/vector+0x2c3d4) (BuildId: c064053e32f2423e7b379105e23b2a12df46daa0)

SUMMARY: AddressSanitizer: heap-use-after-free /home/mgkim/toy-llvm/cpp_practice/cpp-memory/06_vector_invalidation.cpp:21:31 in main
Shadow bytes around the buggy address:
  0x501ffffffd80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x502000000000: fa fa[fd]fa fa fa fd fa fa fa fd fd fa fa fa fa
  0x502000000080: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000100: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000180: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000200: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000280: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==181271==ABORTING
```

이건 -O1로 도 검출됐다.

vector에 push_back하다보면 capacity가 일정이상 찼을 때 더 큰 메모리로 옮긴다.

그래서 p가 이미 해제된 v를 보고있어서 heap-use-after-free 에러가 나게 된 것임.

`v.data()`는 벡터 시작 주소값 반환하는 함수.

reserve같은 거로 vector크기를 미리 정의하면 재할당이 발생하진않으니까 이렇게 해도 될 것 같긴함.

참고로 reserve는 공간만 예약하두는 것이기 때문에 원소가 없는 위치에 접근하면 에러발생하는데, resize는 실제로 확보를 해두는 거라서 범위 내에서 접근가능하다.

### 07

객체가 멤버로 포인터를 갖고있는데, 이게 얕은 복사가 일어나면서 주소만 들고 있게 된다.

이러면 double free 발생.

```bash
Buffer allocated
Buffer freed
=================================================================
==183993==ERROR: AddressSanitizer: attempting double-free on 0x514000000040 in thread T0:
    #0 0x562c702c5161 in operator delete[](void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x106161) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)
    #1 0x562c702c6dcb in Buffer::~Buffer() /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:13:9
    #2 0x562c702c6c88 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:23:1
    #3 0x7f7cbe3fe1c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #4 0x7f7cbe3fe28a in __libc_start_main csu/../csu/libc-start.c:360:3
    #5 0x562c701eb374 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x2c374) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)

0x514000000040 is located 0 bytes inside of 400-byte region [0x514000000040,0x5140000001d0)
freed by thread T0 here:
    #0 0x562c702c5161 in operator delete[](void*) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x106161) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)
    #1 0x562c702c6dcb in Buffer::~Buffer() /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:13:9
    #2 0x562c702c6c74 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:23:1
    #3 0x7f7cbe3fe1c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #4 0x7f7cbe3fe28a in __libc_start_main csu/../csu/libc-start.c:360:3
    #5 0x562c701eb374 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x2c374) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)

previously allocated by thread T0 here:
    #0 0x562c702c4901 in operator new[](unsigned long) (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x105901) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)
    #1 0x562c702c6d2d in Buffer::Buffer() /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:8:16
    #2 0x562c702c6c44 in main /home/mgkim/toy-llvm/cpp_practice/cpp-memory/07_fixed_raii.cpp:19:12
    #3 0x7f7cbe3fe1c9 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #4 0x7f7cbe3fe28a in __libc_start_main csu/../csu/libc-start.c:360:3
    #5 0x562c701eb374 in _start (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x2c374) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf)

SUMMARY: AddressSanitizer: double-free (/home/mgkim/toy-llvm/cpp_practice/cpp-memory/raii_asan+0x106161) (BuildId: 68bad91db9047d49f26885c5720fb52c68a83abf) in operator delete[](void*)
==183993==ABORTING
```

`ERROR: AddressSanitizer: attempting double-free`가 발생한다.

포인터 말고 벡터 쓰면 알아서 내부 메모리 정리하고, deep copy로 복사된다. b1, b2의 두 data가 서로 다른 메모리 주소에 위치해있다는 말이다.

## gdb 써보기

Address sanitizer는 문제를 자동으로 알려준다면, gdb는 디버거다.

```bash
g++ -g -O0 01_use_after_free.cpp -o uaf
gdb ./uaf

Reading symbols from ./uaf...
(gdb) break main
Breakpoint 1 at 0x11d5: file 01_use_after_free.cpp, line 4.
(gdb) run
Starting program: /home/mgkim/toy-llvm/cpp_practice/cpp-memory/uaf 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".

Breakpoint 1, main () at 01_use_after_free.cpp:4
4           int* p = new int(42);
(gdb) next
6           std::cout << "before delete: " << *p << std::endl;
(gdb) next
before delete: 42
12          delete p;
(gdb) print p
$1 = (int *) 0x55555556b2b0
(gdb) print *p
$2 = 42
(gdb) next
13          p = nullptr;
(gdb) print p
$3 = (int *) 0x55555556b2b0
(gdb) print *p
$4 = 1431655787
```

ide에서 중단점찍고 하는거랑 똑같다.

| 명령어           | 의미                 |
| ------------- | ------------------ |
| `break main`  | main에 breakpoint   |
| `run`         | 실행                 |
| `next`        | 다음 줄 실행            |
| `step`        | 함수 안으로 진입          |
| `continue`    | 다음 breakpoint까지 실행 |
| `print x`     | 변수 출력              |
| `print *p`    | 포인터가 가리키는 값 출력     |
| `backtrace`   | call stack 출력      |
| `info locals` | 지역 변수 출력           |
| `quit`        | 종료                 |

컴파일을 좀 편하게 하기 위해서 cmake에 대한 학습도 해야겠다.