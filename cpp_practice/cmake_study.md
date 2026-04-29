CMake는 g++에게 넘길 빌드 명령을 만들어주는 도구.

원래 cpp파일을 g++로 컴파일해서 -o로 나온 실행파일을 실행했다면, CMake는 흐름이 이럼

```text
CMakeLists.txt 작성
   ↓
cmake -S . -B build
   ↓
build 디렉토리에 빌드 시스템 생성
   ↓
cmake --build build
   ↓
실행 파일 생성
```

인데, 

```bash
g++ main.cpp calculator.cpp user.cpp network.cpp -o app
g++ main.cpp calculator.cpp -Iinclude -Llib -lsomething -o app
```

만약에 파일이 여러개가 되고, 라이브러리를 붙여서 써야되면 힘들어진다.

솔직히 sanitizer 쓸 때도 옵션 너무 길다고 생각했다.

그런걸 미리 설정해서 쉽게 실행할 수 있도록하는게 cmake가 하는 일이다.

CMakeList를 만들고, 

```cmake
cmake_minimum_required(VERSION 3.16)

project(HelloCMake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hello main.cpp)
```

빌드하면 된다.

```bash
cmake -S . -B build
cmake --build build
./build/hello
```

맨 윗 줄부터 알아보면

- 이 프로젝트가 요구하는 최소 CMake 버전
- 프로젝트이름
- cpp 17 사용
- cpp 17로 빌드 안하면 에러내기
- main.cpp를 컴파일해서 hello라는 실행파일 만들기

이다.

명령어는 

```text
-S .        source directory는 현재 폴더
-B build    build directory는 build 폴더
```

라서, cmake하고 나면 build폴더가 생성되고 --build로 그 폴더를 빌드한다.

02-multi-file의 CMakeLists파일을 보면 헤더파일은 executable에 안들어가있다.

```cmake
cmake_minimum_required(VERSION 3.16)

project(MultiFile)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(app
    main.cpp
    calculator.cpp
)
```

실제로 컴파일 되는 건 cpp파일이라서 그런듯.

03-include가 중요함.

```cmake
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

이게 app 파일을 컴파일할 때, includㄷ 폴더를 헤더 검색 경로에 추가하라는 의미임.

PRIVATE같은 scope 키워드가 있는데, 내 실행 파일에서만 쓸 include 경로면 PRIVATE, 다른 라이브러리 사용자에게도 알려야 하면 PUBLIC, 헤더만 전달해야 하면 INTERFACE이다.

이걸 보면, CMake에서 실행파일, 라이브러리 모두 target으로 치는데, 이번엔 라이브러리를 만들어보자

```cmake
add_library(calculator
    src/calculator.cpp
)

target_include_directories(calculator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(app PRIVATE
    calculator
)
```

add_library로 라이브러리 화 할 코드를 등록하고, 그걸 app이 사용한다고 link 한다.

근데 calculator에 include 경로를 PUBLIC으로 줬기 때문에 app은 include 경로 몰라도 된다.

## PRIVATE / PUBLIC / INTERFACE

```cmake
target_include_directories(my_lib PRIVATE include)
```

이건 include 폴더가 my_lib을 빌드할 때만 필요하고, my_lib 사용하는 쪽으로 전파하지 않는다.

```cmake
target_include_directories(my_lib PUBLIC include)
```

이건 my_lib을 사용하는 쪽에도 전파된다.

```cmake
target_include_directories(my_lib INTERFACE include)
```

my_lib 자체 빌드시에는 필요없늗네, my_lib 사용하는 쪽이 필요함

.cpp에서만 필요하다 -> PRIVATE
.h에 노출된다(헤더파일의 공개가 필요하다) -> PUBLIC
헤더-only 라이브러리다 -> INTERFACE

디버깅하려면 debug 빌드를 써야된다.

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

release 빌드에는 최적화가 적용되어있어서 디버깅이 어려움.

디버그로 만들어진 실행 파일에 gdb를 켜서 디버깅 가능


