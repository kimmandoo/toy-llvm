https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl02.html

lexer에서 토큰을 쪼갰으니까, 이제 이 토큰으로 문장을 만들어야된다.

컴파일러의 이후 단계가 해석하기 쉬운 구조로 표현하기 위해 Abstract Syntax Tree - AST라고 부르는 구조를 사용한다.

다시 말하면 파서가 만든 구조를 이후 단계가 다루기 쉽게 하려고 AST를 쓴다.

예를 들어 x + y라는 코드가 있다면, "'+'라는 연산을 수행하는데, 왼쪽에는 'x'가 있고 오른쪽에는 'y'가 있다"는 구조로 파악하는 것임.

## The Abstract Syntax Tree (AST)

컴파일러가 해석하는 데 도움을 주는 구조다. 

```cpp
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};
```

sub class로 NumberExprAST를 만들었음. NumberExprAST 인스턴스는 숫자 식 노드자체다.
단순히 “numeric 값이 들어있다” 정도가 아니라, 이 식이 숫자 리터럴이며 그 값이 얼마인지를 표현한다.

이렇게 되면 NumberExprAST가 숫자 리터럴 값을 멤버 변수로 들고 있어서 컴파일 단계에서 그 값을 알 수 있게 된다.

컴파일러가 인식할 수 있는 다른 타입의 인스턴스 들도 만들어주면 아래와 같아진다.

```cpp
/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
};
```

그냥 선언된 그대로 이해하면 되긴한다. 문제는 조건분기가 없어서 튜링-완전하지않다. 이건 나중에 고친다고 하니 기다려보겠다.

BinaryExprAST의 핵심은 단순 선언이 아니라 트리 구조를 포인터로 재귀적으로 연결한다. LHS, RHS가 std::unique_ptr<ExprAST>인 이유는 좌변과 우변도 결국 또 다른 식이기 때문이다.

실제 트리 형태를 생각해보면 좀 이해가 된다.

그래서 1 + 2 * 3 같은 것도 노드 안에 노드를 넣는 식으로 표현된다.

연산이나 값, 호출같은 의미를 알 수 있게 됐으니 이제 함수에 대한 AST가 필요하다.

```cpp
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
};
```

PrototypeAST가 함수 이름과 인자 이름들을 저장하고있는데 타입의 경우는 어차피 사용할 값이 double이라 신경쓸 필요가 없나보다. 혹시라도 string을 떠올렸다면, 그건 식별자 이름을 저장하는 용도라 예외다.

타입 시스템 관점에서는 인자 개수만 중요하고, AST 데이터로는 이름까지 저장하고 있는 걸로 보인다.

만약에 ExprAST에서 타입도 들고 있었으면 우리가 아는 함수의 형태가 된다.

파서에 대해 더 깊게 들어가기 전에... 난 cpp 숙련도가 부족해서 전부 다 짚고 넘어가야한다.

```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
};
```
클래스 선언이다.

public 은 접근 지정자이고, public/private/protected가 있는데 cpp은 기본이 private라 public을 명시해준 것이다.

그러니까, cpp에서 class의 멤버 접근자 기본이 private이고, 만약 struct라면 public이 기본이다.

`~ExprAST()`는 소멸자를 의미한다. 객체가 사라질 때 호출된다.

근데 좀 특이하다. virtual 소멸자를 쓴다.

ExprAST는 부모 클래스고, NumberExprAST, VariableExprAST 같은 자식 클래스가 상속받고 있는데, 아래와 같은 경우를 생각해보자.

```cpp
ExprAST* p = new NumberExprAST(3.14);
delete p;
```

cpp에서는 객체를 delete로 제거함.

위 코드에서 부모 포인터로 자식 객체를 가리킬 수도 있지않나? virtual이 아니면 자식 소멸자가 제대로 안불릴 수 있다.

그래서 상속용 베이스 클래스는 보통 가상 소멸자를 둔다. 이 포인터의 정적 타입은 부모지만, 실제 객체 타입 기준으로 소멸시켜라”라는 의미이다.

default를 배정한건 컴파일러가 기본동작을 지정하게 한 것이다. “이 클래스는 소멸자 자체는 필요하지만, 특별히 내가 정리할 일은 없다”라는 의미다.

```cpp
virtual ~ExprAST() = default;
virtual ~ExprAST() {}
```
소멸자 구현에는 필요에 따라 자원 정리 로직같은게 들어갈테지만 그게 아닐경우 빈 중괄호 블럭보다 default가 나아보인다.

```cpp
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};
```

상속이다. 자바로 변환하면 `class NumberExprAST extends ExprAST` 쯤 되지않을까?

상속 방식도 접근 수준이 있다.

- public 상속
- protected 상속
- private 상속

is-a 관계면 public 상속을 쓴다.

`double Val;`은 멤버 변수다. 

`NumberExprAST(double Val) : Val(Val) {}`는 생성자다. NumberExprAST객체를 만들때 double 하나를 매개변수로 받아서 멤버변수 Val에 넣겠다는 의미다.

`Val(Val){}` 가 좀 어지러운 부분인데, 초기화리스트라고 부른다.

```cpp
클래스명(매개변수) : 멤버(값), 멤버(값) {
    // 본문
}
```
으로 이해해야된다.

매개변수가 값에 대응한다. 좀 더 이해하기 쉽게 본문에다가 초기화 시키면

```cpp
NumberExprAST(double Val) {
    this->Val = Val;
}
```

이렇게 된다.

본문으로 쓰는 경우도 있겠지만 const 멤버, 참조 멤버, 기본 생성자가 없는 멤버, 효율이 중요한 객체들은 초기화 리스트가 정석이라고 한다...

왜 그러냐면, 초기화랑 대입이 다르게 동작하기 때문이다.

```cpp
NumberExprAST(double Val) : Val(Val) {}
NumberExprAST(double Val) {
    this->Val = Val;
}
```

초기화 리스트가 처음부터 그 값으로 멤버 변수를 생성한다 치면, 본문에서 대입으로 처리할 경우 멤버 변수 생성 후 매개변수를 받아 대입이라는 중간 절차가 생긴다.

또 const는 대입이 안되고, 참조도 한번 묶이면 바꿀 수 없는 상태고, 기본 생성자가 없는 경우에는 멤버를 빈상태로 둘 수 없기때문에 초기화 리스트가 정석처럼 받아들여지는 것 같다.

```cpp
VariableExprAST(const std::string &Name) : Name(Name) {}
```

`&`는 참조다. 문자열을 복사해서 쓰는게아니라 원문 그 자체를 받아서 쓴다는 의미다.

std::string은 복사 비용이 있을 수 있다. 그래서 매개변수로 받을 때 매번 복사하지 않고 참조로 받으면 효율적이다.

```cpp
void f(std::string s);         // 복사
void f(std::string& s);        // 참조
void f(const std::string& s);  // 읽기 전용 참조
```

이해하고 나니까 상수를 선언하는 const 예약어를 붙이고 &로 참조시키는 건 효율적으로 보인다. -> 복사는 싫고, 수정도 싫은데 읽게는 하고 싶다.

```cpp
std::unique_ptr<ExprAST> LHS, RHS;
```

스마트 포인터.. 라는 처음보는 친구다.

기존에 아는 `ExprAST* p;`는 raw pointer라고 지칭하며 new, delete 관리를 해줘야했다.

이게 스마트 포인터가 생겨난 배경인가 보다.

`std::unique_ptr<T>`로 선언하며, 어떤 객체를 단 하나의 소유자만 갖도록 하겠다라는 의미다. 어떻게 보면 rust에서 나오는 소유권과 비슷한 것도 같다??

즉 이 포인터가 객체 생명주기까지 책임진다.

```cpp
std::unique_ptr<ExprAST> p = std::make_unique<NumberExprAST>(1.0);
```

delete를 직접하지않아도 객체가 파괴되면 알아서 delete 된다.

그럼 다시 BinaryExprAST로 와서

```cpp
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;
```
이게 왜 스마트포인터를 쓸까?

`BinaryExprAST('+', VariableExprAST("x"), VariableExprAST("y"))`

뭐 기존의 포인터로도 손색없기는 한데.. 만약 이 값을 여러군데에서 사용한다면 어지러운 상황이 발생한다.


```cpp
ExprAST* LHS;
ExprAST* RHS;
```

이렇게 되어있다고 하면, 사용이 끝나고 누가 지워야 하는지가 애매하다.

BinaryExprAST가 지우나? 다른 누군가도 참조하나? 중간에 예외 나면 누수 안 나나? 모든 경우를 생각해야될 수 도 있는데, unique_ptr로 걱정을 덜었다.

```cpp
BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
              std::unique_ptr<ExprAST> RHS)
  : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
```

LHS, RHS를 보면 초기화 리스트로 std 함수들을 넘기고 있다.

unique_ptr는 소유자가 하나만 있어야 하기 때문에 복사가 안된다.

```cpp
std::unique_ptr<ExprAST> a = ...;
std::unique_ptr<ExprAST> b = a; // 오류
```

이런건 a에 대한 소유권이 a, b로 나누어지니까 안된다는 얘기다.

대신 std::unique_ptr<ExprAST> b = std::move(a); 를 사용하면 a가 갖고있던 소유권을 b로 넘기라는 의미가 되고, a는 비어있게 된다.

그럼 왜 생성자에서 move를 하지?

이건 참조를 쓸때랑 비슷하게 보면 된다. 그냥 = 해보리면 복사가 일어나는데, 스마트 포인터는 복사가 안되기 때문에 move로 처리한다.

생성자 인자로 들어온 LHS의 소유권을 멤버 변수 LHS로 넘기는 형태가 된다.

좀 더 파고들어가 보면,

```cpp
std::unique_ptr<ExprAST> lhs = ...;
std::unique_ptr<ExprAST> rhs = ...;

auto node = std::make_unique<BinaryExprAST>('+', std::move(lhs), std::move(rhs));
```

여기서 node안의 멤버 lhs, rhs가 실제 자식 노드를 갖고 있을 것이고, 그 자식노드들은 비어있는데 자식의 소유권을 부모인 node에게 넘겨주는 형태다.

여기서 다시한번 unique_ptr가 자연스러운 이유를 알 수 있는데, 부모가 여러명일 경우가 없기 때문이다.

```cpp
std::vector<std::unique_ptr<ExprAST>>
```

cpp의 제네릭 문법이다.

vector가 자바의 ArrayList에 대응하는 걸 생각하면 ArrayList<String>와 거의 같다.

```cpp
CallExprAST(const std::string &Callee,
            std::vector<std::unique_ptr<ExprAST>> Args)
  : Callee(Callee), Args(std::move(Args)) {}
```
여기서는 벡터안에 스마트 포인터가 들어가 있다. 그래서 그냥 복사가 안되니까 move로 처리해준다.

```cpp
const std::string &getName() const { return Name; }
```

const가 눈을 현혹시킨다.

`const std::string &`이 반환타입인데, 반환값을 문자열 복사본이 아니라 읽기 전용 참조를 의미하고,

함수 뒤의 const는 이 멤버 함수가 객체 상태를 바꾸지않는 다는 뜻이다.

즉 이 함수안에서 멤버 변수 수정이 일어나면 안된다.

```cpp
class A {
  int x, y;
public:
  int getX() const { return x; } // 가능
  int getY() const {
    y = 10; // 오류
  }   
};
```

반환값은 매개변수 타입 붙일 때랑 동일하게 생각하면 된다. 읽기로만 쓸꺼니까 불필요한 복사가 일어나지않도록 const + &로 처리한 것이다.

참조를 계속 쓰는데, 수명관리가 핵심이다.

제일 기초를 빼먹을 뻔.

매개변수가 없는 생성자를 기본 생성자라고 하는데 꼭 넣어줘야한다.

```cpp
class A {
public:
  A() {}
  // 또는 그냥 A a;
  // 또는 A() = default;
};
```

근데 cpp는 기본생성자 자동으로 컴파일러가 만들어주긴함. 근데 일반 생성자를 만들게 되면 컴파일러가 기본생성자를 안만들어주기 때문에 주의해야된다.

오히려 해당 방식으로만 객체를 생성하도록 강제하는 방법이니까 더 안전한 방법이 아닐까?

```cpp
NumberExprAST(double Val) {
    this->Val = Val;
}
```
this는 현재 객체 자신을 가리키는 포인터.

멤버 Val이랑 매개변수 Val이 헷갈리니까 멤버 변수 Val을 `this->` 로 명시해준 것.

- 기본 생성자: A()
- 일반 생성자: A(int x)
- 복사 생성자: A(const A& other)
- 이동 생성자: A(A&& other)

## Parser Basics

이제 AST도 준비됐으니까 parser를 만들면 되는 데, 그 전에 몇가지 헬퍼 함수를 만들어보자.

token화 이후, AST 구조로 받아서, 그걸 parser에서 처리하는 것이다.

“x+y”가 주어지면 아래 코드 처럼 호출되기를 원한다.

```cpp
auto LHS = std::make_unique<VariableExprAST>("x");
auto RHS = std::make_unique<VariableExprAST>("y");
auto Result = std::make_unique<BinaryExprAST>('+', std::move(LHS),
                                              std::move(RHS));
```

auto는 변수 타입을 컴파일러가 알아서 추론하는 타입이다. auto가 js처럼 런타임때 잡히는 게 아니라 컴파일타임때 확정되기 때문에 반드시 초기값이 필요하다.

```cpp
int a = 10;
auto& x = a;
```
이 때 x는 `int&` 타입이고,
```cpp
const std::string s = "hello";
const auto& x = s;
```
이 때는 `std::string&` 타입이다.

추론이 가능하다고 그냥 막 auto 때려버리면 쉽지않기 때문에 우항을 보고 사람도 타입을 확정지을 수 있을 때 auto를 쓰는게 좋은 습관이라고 한다.

다시 parser로 돌아와서, 토큰을 먼저 받아보자.

```cpp
/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
  return CurTok = gettok();
}
```
step1때 구현했던 gettok 메서드를 떠올려보면, Curtok에는 항상 우리가 파싱해야될 토큰이 들어올 것이다.

```cpp
/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}
```

Expr 타입에 대한 LogError, Prototype에 대한 LogError를 선언해줬다.

## Basic Expression Parsing
제일 간단한 숫자 파싱 먼저 해보겠다.

```cpp
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}
```

숫자는 NumVal 전역변수에 저장되어있는데, 그걸 사용한다. 현재 숫자 토큰값을 받아 AST 노드를 만들고 다음 토큰을 가져온 뒤(지금 토큰을 소비했다는 의미로) 포인터 소유권을 호출부로 넘긴다.

괄호 연산이라면 이렇게 할 수 있다

```cpp
/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}
```

닫는 괄호가 없을 경우에는 LogError를 발생시킨다.

그리고 앞으로 계속 나올거긴한데, ParseExpression이 재귀로 돈다.

```cpp
/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken();  // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken();  // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}
```

이번엔 tok_identifier에 집중해야된다. gettok에서 알파벳으로 시작해 문자나 알파벳이 계속 나올때, tok_identifier = -4를 반환했다.

std::make_unique는 unique_ptr을 안전하게 생성하는 함수.

```cpp
auto p = std::make_unique<Type>(args...);
```
이렇게 생긴건, Type객체를 new로 만들고 그걸 unique_ptr로 감싸서 p에 반환한다는 의미.

아무튼 ParseIdentifierExpr의 중요한 건 현재 식별자가 독립 변수 참조인지 함수 호출 표현식인지 확인하기 위해 룩어헤드를 사용한다는 점이다.

```cpp
/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}
```

위에서 만든 것들이 여기에서 호출된다. CurTok의 상태에 따라 Expr 메서드들을 더 다양하게 늘리게될 수 있다.

## Binary Expression Parsing

바이너리 표현식은 아무래도 좀 어렵다.

`x+y*z` 가 주어진다고 했을때, 이걸  “(x+y)*z” 인지 “x+(y*z)” 인지 판단해야된다. 근데 사람은 이미 곱셈의 우선도가 덧셈보다 높다는 걸 알고있다.

```cpp
/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.
  ...
}
```

우선도를 저장해서, 이걸 파싱 순서에 넣을 것이다. 일단 4개 연산자만 각각 우선도를 갖고있고 나머지는 바이너리 연산자라고 받아들이지않아서 -1를 반환한다.

```cpp
/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```
토큰 소비 순서를 보면 재밌다.

하나의 expression은 primary expression 하나와 이어지는 [연산자, primary] 쌍들의 연속으로 구성된다.

a + b * c + d는  `a   [+ b]   [* c]   [+ d]`로 분리된다.

먼저 primary 하나 파싱한뒤 그걸 LHS로 넘기고 나머지 연산을 ParseBinOpRHS에서 처리하고 있다.

```cpp
/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (true) {
    int TokPrec = GetTokPrecedence(); // 현재 연산자 우선 순위

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;
```

if문이 연산자 비교 조건이다. 지금 연산자 우선순위가 너무 낮으면 연산하지 않고 여기서 파싱 끝내고 반환하는 구조다.
```cpp
// Okay, we know this is a binop.
int BinOp = CurTok;
getNextToken();  // eat binop

// Parse the primary expression after the binary operator.
auto RHS = ParsePrimary();
if (!RHS)
  return nullptr;
```

연산자를 저장하고, 저장했으니 다음 토큰 불러다가 오른쪽 primary 값을 소비하는 구조다.
```cpp
// If BinOp binds less tightly with RHS than the operator after RHS, let
// the pending operator take RHS as its LHS.
int NextPrec = GetTokPrecedence();
if (TokPrec < NextPrec) {
```

만약에 지금 연산자보다 다음 연산자의 우선도가 높다면?

a + b * c
 -> a, [+ b], [* c]

b랑 c를 먼저 묶어야됨.

a + b * c * d
 -> `RHS = (b * c * d)`

```cpp
    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    // Merge LHS/RHS.
    LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                                           std::move(RHS));
  }  // loop around to the top of the while loop.
}
```
지금은 재귀호출 구조로 구현되어있고 지금보다 우선순위 높은 연산들을 전부 RHS로 묶어서 파싱한다.

## Parsing the Rest

이제 함수의 prototype을 다룰 차례다.

우리 언어에서는 prototype이 extern나 함수 본문이 있는 def 모두에 적용된다.

```cpp
/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  // Read the list of argument names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}
```

공백으로 매개변수를 구분하는 함수를 만들거다.

먼저 현재 토큰이 tok_identifier가 아니면 “프로토타입에서 함수 이름이 와야 한다”는 에러를 내고 종료한다.

토큰을 받았으면 식별자 이름을 함수이름으로 저장하고 넘어간다.

토큰 종류랑, 토큰 값이 분리되어있다는 점을 알고 가야된다.

CurTok == tok_identifier 는 현재 토큰 종류가 식별자임을 판별하고 IdentifierStr 는 그 식별자의 실제 문자열 값이 뭔지를 판별한다.

함수로 이제 인식됐으면 인자들을 받는다.  벡터로 들어있고, 사이즈가 따로 정해져있지 않기 때문에 다음 토큰을 읽었을 때 식별자인 동안 push_back으로 각 인자의 그 이름을 넣어준다.

함수 이름과 인자 이름 목록을 이용해서 PrototypeAST를 만들어 반환한다.

```cpp
/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();  // eat def.
  auto Proto = ParsePrototype();
  if (!Proto) return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}
```

이제 프로토타입 하나와, 함수 본문을 구현하는 expression 하나를 붙이면 함수가 만들어진다.

def를 날리고, 함수 시그니처를 소비하기 위해 ParsePrototype을 호출한다. expression 파싱이 성공하면 프로토타입과 expression을 묶어 FunctionAST를 들고, 실패하면 nullptr를 반환한다.

그러니까 함수는 결국 PrototypeAST랑 본문 ExprAST로 구성된다.

```cpp
/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}
```

extern도 토큰이 분리되어있으니 이거까지 보고가자. 이제 extern은 본문이 없는 프로토타입일 뿐이다.

뭐 대충 `double sin(double);`에 가까운 느낌일 것같다.

```cpp
/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}
```

마지막으로, 사용자가 임의의 top-level expression을 직접 입력해서 바로 평가할 수도 있게 할 것이다.
이건 인자가 0개인 함수를 만들어 처리한다.

그러니까, 4 + 5같은 expression을 `def __anon_expr() 4 + 5` 로 만들어준다는 얘기다. 실제로 ProtoTypeAST를 만들어 넘기는 곳의 vector가 비어있다.

사용하는 방법을 공통화 시키기 위해 추상화를 하는 것에 가깝다고 느껴진다.

## The Driver

파서를 열심히 만들었으니 입력을 받아 토큰을 받고, 그걸로 적절한 파싱함수를 호출해줄 Driver를 만들어보자.

```cpp
/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}
```

입력이 끝났으면 루프를 종료하고, 세미콜론이면 따로 AST에서 처리할 게 없으니 그냥 소비하고, def, extern은 각각 처리해주고, 나머지는 top-level로 처리하는 driver다. 

### namespace

추가로, 코드를 보면 익명 namespace를 선언하고 거기에 class들을 선언한다.

```cpp
namespace {
  class ExprAST { ... };
  class NumberExprAST : public ExprAST { ... };
  ...
}
```

이건 “이 클래스들은 이 파일 내부 구현용이니 밖에서 보지 마라” 라는 의미다.

혹시라도 다른 파일에 존재할 같은 이름 함수와 중복되는 위험을 피할 수 있다는 것도 장점이다.

static으로 선언해도 될 것 같은데, 이게 쓸게 많으면 namespace로 지정하는 게 더 좋다고 한다. 

근본으로 돌아와서 먼저 익명 namespace 말고, 그냥 namespace부터 알아 보겠다.

```cpp
namespace Math {
  int add(int a, int b) {
    return a + b;
  }
}
```
이렇게 선언하면 add라는 함수는 Math::add(1,2) 처럼 써야된다.

이름 충돌을 막는 다는 얘기도 여기에 있다.

```cpp
namespace A {
  void print() {}
}

namespace B {
  void print() {}
}
```

이러면 같은 함수지만, namespace가 구분되어있어서 A::print(), B::print() 로 사용해야된다. 그리고 익명 namespace와는 다르게 다른 파일에서도 접근 가능하다.

정리하면 아래와 같다.

- namespace A { ... }     -> 이름 붙은 그룹
- namespace { ... }       -> 이 파일 안 전용 그룹
- static void foo()       -> 이 파일 안 전용 함수
- private                 -> 이 클래스 안 전용 멤버

