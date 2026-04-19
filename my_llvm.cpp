#include <iostream>
#include <string>
#include <vector>

// 1. 토큰 정의 [cite: 31]
enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; 
static double NumVal;             

// 2. 핵심 함수: gettok() [cite: 32]
static int gettok() {
  static int LastChar = ' ';

  // 공백 무시 [cite: 33]
  while (isspace(LastChar))
    LastChar = getchar();

  // 식별자(def, extern 등) 인식 [cite: 33]
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def") return tok_def;
    if (IdentifierStr == "extern") return tok_extern;
    return tok_identifier;
  }

  // 숫자 인식 
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  // 주석 처리 (#으로 시작) 
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // 파일 끝 처리
  if (LastChar == EOF)
    return tok_eof;

  // 그 외 문자(+, - 등)는 ASCII 값 그대로 반환
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

// 3. 테스트용 메인 함수
int main() {
  while (true) {
    int tok = gettok();
    if (tok == tok_eof) break;
    std::cout << "Got token: " << tok << std::endl;
  }
  return 0;
}