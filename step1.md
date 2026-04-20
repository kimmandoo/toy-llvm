
https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html

kaleidoscope라는 언어를 만들것이다.

피보나치 함수가 파이썬같은 코드로 나오는데, 이런 걸 처리할 수 있게 만드나 보다.

```code
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)
```

## Lexer 역할

컴파일러의 첫 번째 작업은 소스 코드를 읽어서 토큰이라 불리는 작은 조각으로 나누는 것이다. def라는 글자를 읽으면 이것이 '함수 정의'를 뜻하는 키워드임을 인식하는 것이 Lexer의 역할이 되겠다.

```cpp
// Lexer가 반환할 토큰 종류들
enum Token {
  tok_eof = -1,           // 파일의 끝

  // 명령어(Keywords)
  tok_def = -2,           // 'def'
  tok_extern = -3,        // 'extern'

  // 식별자 및 숫자
  tok_identifier = -4,    // 변수명, 함수명 등
  tok_number = -5,        // 숫자
};

static std::string IdentifierStr; // tok_identifier일 때 이름 저장
static double NumVal;             // tok_number일 때 숫자 값 저장
```

이게 내 언어에서 사용될 토큰들이다.

아래 함수는 표준 입력에서 문자를 하나씩 읽어 토큰을 반환하는 역할을 한다. 토큰 분류기다.

```cpp
#include <iostream>
#include <string>
#include <vector>

enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; 
static double NumVal;             

static int gettok() {
  static int LastChar = ' '; // static으로 사용해서 마지막 문자를 기억하게 하는 것

  while (isspace(LastChar))
    LastChar = getchar(); // 공백이면 그냥 계속 받기 -> 무시나 마찬가지

  if (isalpha(LastChar)) { // 알파벳이면?
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar; // 그 토큰 뒤에오는 모든 알파벳, 숫자를 이어붙인다. -> 공백오면 끊긴다.

    if (IdentifierStr == "def") return tok_def; // 그게 def면 def 토큰(-2)
    if (IdentifierStr == "extern") return tok_extern; // 그게 extern이면(-3) 반환
    return tok_identifier; // 그 외에는 -4
  }

  // 숫자 인식  - 숫자가 처음오면 double형으로 바꾼다.
  if (isdigit(LastChar) || LastChar == '.') { // 문자가 숫자(0~9)이거나 소수점(.)이면 숫자로 인식
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');
    // 일단 다 이어붙이고
    // 실제 숫자 값으로 변환함. strtod
    NumVal = strtod(NumStr.c_str(), 0); // 숫자는 NumVal로
    return tok_number; // -5
  }

  // 주석 처리 (#으로 시작) 
  if (LastChar == '#') { // 주석은 그냥 줄바꿈이 나올때까지 토큰 패싱
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok(); // 재귀 호출해서 주석 뒤에나오는 토큰으로
  }

  // 파일 끝 처리
  if (LastChar == EOF)
    return tok_eof;

  // 그 외 문자(특수문자 같은거)는 그대로 반환
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

## 컴파일 및 실행

```bash
clang++ -g -O3 my_llvm.cpp -o out/my_llvm
```

03이 아니라 Optimization의 O를 사용한 O3이다.

!["image"]("./img/image.png")

컴파일러 옵션 `-O` 뒤에 붙는 숫자가 커질수록 컴파일러는 코드를 더 많이 고민하고 분석한다.

| 옵션 | 단계 | 설명 |
| :--- | :--- | :--- |
| `-O0` | 최적화 없음 | 기본값. 코드를 작성한 그대로 기계어로 옮깁니다. 컴파일 속도는 가장 빠르지만 실행 속도는 가장 느리며, 디버깅할 때 변수 값이 코드와 일치하여 분석하기 좋다. |
| `-O1` | 기본 최적화 | 코드 크기나 컴파일 시간을 크게 늘리지 않는 선에서 아주 기본적인 최적화만 수행 |
| `-O2` | 권장 최적화 | 대부분의 배포용 소프트웨어에서 사용하는 표준 단계. 실행 속도를 높이기 위해 거의 모든 최적화 기법을 동원하지만, 실행 파일의 크기가 너무 커지지 않도록 조절. |
| `-O3` | 최대 최적화 | 실행 속도를 최대로 높이기 위해 공격적인 기법(루프 펼치기, 함수 인라이닝 등)을 사용. 실행 속도는 가장 빠르지만, 결과 파일의 크기가 커질 수 있다. |

대표적인 최적화 옵션은 아래와 같이 동작한다.

* 상수 접기 (Constant Folding): `3 + 5` 같은 코드가 있다면, 프로그램 실행 중에 계산하는 대신 컴파일 단계에서 미리 `8`로 바꿔버림.
* Dead Code Elimination: 절대 실행될 리 없는 `if (false)` 블록 같은 코드를 삭제하여 파일 크기를 줄임.
* 함수 Inlining: 자주 호출되는 짧은 함수를 호출하는 대신, 그 함수의 내용을 호출 위치에 직접 박아 넣어 함수 호출에 드는 비용(오버헤드)을 제거.