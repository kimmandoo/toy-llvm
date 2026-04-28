https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl07.html

LLVM은 입력코드가 SSA form이어야하는데, 프론트엔드가 직접 SSA form을 만들 필요는 없다.

프론트엔드는 사용자가 작성한 소스 코드를 읽어서 토큰화, 파싱, AST 생성, LLVM IR 생성까지 담당하는 부분으로, 기계어 변환을 위해 최적화 단계 직전까지라고 보면 된다.

SSA를 쓰기 때문에 변경가능한 변수가 복잡도를 항상 늘린다.

```cpp
int G, H;

int test(_Bool Condition) {
  int X;
  if (Condition)
    X = G;
  else
    X = H;
  return X;
}
```

5장에서 phi노드가 처리하는 방식을 공부했다. 

이 간단한 코드에서도, X가 IR로 변환되면 각기 다른 x0,1,2로 생성된다

```llvm
@G = weak global i32 0   ; type of @G is i32*
@H = weak global i32 0   ; type of @H is i32*

define i32 @test(i1 %Condition) {
entry:
  br i1 %Condition, label %cond_true, label %cond_false

cond_true:
  %X.0 = load i32, i32* @G
  br label %cond_next

cond_false:
  %X.1 = load i32, i32* @H
  br label %cond_next

cond_next:
  %X.2 = phi i32 [ %X.1, %cond_false ], [ %X.0, %cond_true ]
  ret i32 %X.2
}
```

PHI는 일반적인 함수 호출이 아니라 “분기들이 다시 합쳐질 때 어떤 값을 사용할지 결정하는 SSA 전용 명령”인 점 다시 상기하고 들어가자

# 7.3. LLVM에서의 메모리

LLVM은 모든 레지스터 값이 SSA form이기를 요구하지만, 메모리 객체는 SSA form으로 표현하지 않는다.

데이터 흐름을 ir에 인코딩하지않고 필요할 때 계산 하도록 analysis pass를 통해 처리한다.

이걸 이용하면 다음과 같은 아이디어를 떠올릴 수 있게된다.

함수 안의 각각의 변경 가능한 객체마다 스택 변수를 만들고, 그 스택 변수는 스택에 존재하므로 메모리에 있는데, llvm에서 메모리 접근을 load, store로 할 수 있는 걸 사용해서 처리해볼 것이다.

```llvm
@G = weak global i32 0   ; type of @G is i32*
@H = weak global i32 0   ; type of @H is i32*
```

변수 정의가 실제로는 i32*로 되어있다. 즉 @G는 전역 데이터 영역에 i32를 위한 공간을 정의하고 @G라는 이름이 그 공간의 주소를 가리키고 있다.

스택 변수도 같은 방식으로 동작한다.

```llvm
define i32 @example() {
entry:
  %X = alloca i32           ; type of %X is i32*.
  ...
  %tmp = load i32, i32* %X  ; load the stack value %X from the stack.
  %tmp2 = add i32 %tmp, 1   ; increment it
  store i32 %tmp2, i32* %X  ; store it back
  ...
```

전역 변수 정의로 선언하는거 대신 alloca로 선언하는 게 다르다. alloc을 쓰면 스택 슬롯의 주소를 함수에 전달할 수도 있고, 다른 변수에 저장할 수도 있다.

맨 처음에 예시 llvm 코드를 재구성하면 이렇게 바꿀수 있다.

```llvm
@G = weak global i32 0   ; type of @G is i32*
@H = weak global i32 0   ; type of @H is i32*

define i32 @test(i1 %Condition) {
entry:
  %X = alloca i32           ; type of %X is i32*.
  br i1 %Condition, label %cond_true, label %cond_false

cond_true:
  %X.0 = load i32, i32* @G
  store i32 %X.0, i32* %X   ; Update X
  br label %cond_next

cond_false:
  %X.1 = load i32, i32* @H
  store i32 %X.1, i32* %X   ; Update X
  br label %cond_next

cond_next:
  %X.2 = load i32, i32* %X  ; Read X
  ret i32 %X.2
}
```

SSA 대신 메모리 주소를 활용해 임의의 변경 가능한 변수를 PHI 노드 없이 처리할 수 있게 된 것이다.

- 변경 가능한 변수 하나는 스택 할당 하나
- 변수를 읽는 것은 스택에서 load
- 변수를 갱신하는 것은 스택에 store
- 변수의 주소를 얻는 것은 그냥 스택 주소를 직접 사용하면 됨

근데 단순한 연산인데 스택 공간에 너무 많은 접근이 일어난다고 생각할 수 있다. 이걸 mem2reg라는 최적화 pass로 처리할 수 있다.

mem2reg는 이런 alloca들을 SSA 레지스터로 승격시키고, 필요한 위치에 PHI 노드를 삽입한다. 

1. alloca를 찾아서 처리할 수 있으면 승격시킨다. 다만 전역 변수나 힙 할당에는 적용되지 않는다.
2. mem2reg는 함수의 entry block에 있는 alloca 명령만 본다. 
3. mem2reg는 사용이 직접적인 load와 store인 alloca만 승격한다. 이 말은 스택 객체 주소가 함수에 전달되거나, 이상한 포인터 연산이 있으면 승격되지않는다는 의미.
4. mem2reg는 first class value에 대한 alloca에만 동작하고, mem2reg는 구조체나 배열을 레지스터로 승격할 수 없다.

mem2reg에 흔한 케이스를 처리하기 위한 특수 케이스 들이 내장되어있기 때문에 이렇게 승격시키는 작업이 직접 SSA를 처리하는 것보다는 빠르다. 

# 7.4. Kaleidoscope의 변경 가능한 변수

이제 이 메모리 트릭을 써서 mutable한 변수을 만들어볼 것이다.

```llvm
# Define ':' for sequencing: as a low-precedence operator that ignores operands
# and just returns the RHS.
def binary : 1 (x y) y;

# Recursive fib, we could do this before.
def fib(x)
  if (x < 3) then
    1
  else
    fib(x-1)+fib(x-2);

# Iterative fib.
def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;

# Call it.
fibi(10);
```

변수를 변경하려면 기존 변수들도 “alloca 트릭”을 사용하도록 바꿔야 한다.

근데 "Kaleidoscope는 원래 모든 것이 표현식 중심입니다. C처럼 문장 statement가 강하게 나뉘어 있지 않습니다. 그래서 여러 연산을 순서대로 실행하고 싶을 때 : 같은 연산자를 만들어 사용합니다." <- 이게 무슨말?

# 7.5. 기존 변수를 변경 가능하게 조정하기

지금 kaleidoscope에서 심볼테이블은 전역 map으로 관리되고있다. 이 맵은 현재 이름 있는 변수가 가지고 있는 double 값을 담은 LLVM Value*를 추적하는데, 이걸 변수 값 자체가 아니라 메모리 위치를 저장하게 하는 방식으로 변경가능하게 만들 수 있다.

`static std::map<std::string, AllocaInst*> NamedValues;` Value* 대신 AllocaInst*를 가리키게 바꾸면 alloca값을 넣을 수 있다.

```cpp
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          StringRef VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                 TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr,
                           VarName);
}
```

이건 entry block에 alloca가 생성되도록 보장하는 헬버함수다. entry block의 첫 번째 명령을 가리키는 IRBuilder 객체를 만들고 있다. 그걸 VarName으로 만들어서 alloca를 반환하는데, 지금 만드는 언어에는 type이 double 뿐이라 타입을 전달할 필요는 없다.

```cpp
Value *VariableExprAST::codegen() {
  // Look this variable up in the function.
  AllocaInst *A = NamedValues[Name];
  if (!A)
    return LogErrorV("Unknown variable name");

  // Load the value.
  return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}
```

바꾼방식에서는 변수의 위치가 전역에서 스택으로 옮겨졌다. 그래서 스택 슬롯에서 load해올 필요가 있다. 코드를 보면 alloca 주소를 써서 load해오는 걸 알 수 있다.

## 이제 기존 코드를 리팩토링해볼 시간

```cpp
Function *TheFunction = Builder->GetInsertBlock()->getParent();

// Create an alloca for the variable in the entry block.
AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

// Emit the start code first, without 'variable' in scope.
Value *StartVal = Start->codegen();
if (!StartVal)
  return nullptr;

// Store the value into the alloca.
Builder->CreateStore(StartVal, Alloca);
...
```

변수를 정의하던 부분에서 alloca를 사용하고 있다. 반복문 종료조건 계산 이후 after에는 다음 값의 주소를 참조하게 한다.

```cpp
// Compute the end condition.
Value *EndCond = End->codegen();
if (!EndCond)
  return nullptr;

// Reload, increment, and restore the alloca.  This handles the case where
// the body of the loop mutates the variable.
Value *CurVar = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca,
                                    VarName.c_str());
Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
Builder->CreateStore(NextVar, Alloca);
...
```

코드가 거의 바뀐 건 없지만 phi 노드를 만들 필요가 없어졌다는 점이 큰 차이점이다. 함수인자에 대해서도 변경 가능하게 하려면 그거도 alloca를 해주면 된다.

```cpp
Function *FunctionAST::codegen() {
  ...
  Builder->SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    // Create an alloca for this variable.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

    // Store the initial value into the alloca.
    Builder->CreateStore(&Arg, Alloca);

    // Add arguments to variable symbol table.
    NamedValues[std::string(Arg.getName())] = Alloca;
  }

  if (Value *RetVal = Body->codegen()) {
    ...
```

이제 좀 잘 보인다.

각 인자에 대해 alloca를 만들고, 함수에 들어온 입력 값을 그 alloca에 저장한다음 해당 alloca를 인자의 메모리 위치로 심볼테이블에 저장한다.

변수 생성방식을 바꿨으니까 mem2reg로 SSA 최적화를 해주기 위해 pass들을 추가한다.

```cpp
// Promote allocas to registers.
TheFPM->addPass(PromotePass());
// Do simple "peephole" optimizations and bit-twiddling optzns.
TheFPM->addPass(InstCombinePass());
// Reassociate expressions.
TheFPM->addPass(ReassociatePass());
```

mem2reg를 안해도 코드생성은 되지만, 불필요한 변수 재정의가 계속발생한다.

```llvm
entry:
  %x1 = alloca double
  store double %x, double* %x1
  %x2 = load double, double* %x1
  %cmptmp = fcmp ult double %x2, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %ifcond, label %then, label %else

entry:
  %cmptmp = fcmp ult double %x, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %ifcond, label %then, label %else
```

이 두 코드는 완전히 같다.

`NamedValues["x"]`가 x의 스택 슬롯 주소가 되면서 x를 읽을때는 load 스택슬롯, 대입할때느 store 새값 -> 스택슬롯이 된다.

# 7.6. 대입 연산자

연산자이기 때문에 우선순위를 BinopPrecedence에 mapping 해주는게 1순위다.

```cpp
Value *BinaryExprAST::codegen() {
  // Special case '=' because we don't want to emit the LHS as an expression.
  if (Op == '=') {
    // This assume we're building without RTTI because LLVM builds that way by
    // default. If you build LLVM with RTTI this can be changed to a
    // dynamic_cast for automatic error checking.
    VariableExprAST *LHSE = static_cast<VariableExprAST*>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
```

이항 연산자긴 하지만 왼쪽, 오른쪽 코드 생성 후 작업은 아니다. LHS가 반드시 변수여야하는 것도 제약사항이다.

예를 들어서 x = 4라고 하면, x는 x의 현재값이 필요하지않고, x가 저장된 위치만 있으면 된다. 그래서 왼쪽항 codegen을 하지않는 것.


```cpp
  // Codegen the RHS.
  Value *Val = RHS->codegen();
  if (!Val)
    return nullptr;

  // Look up the name.
  Value *Variable = NamedValues[LHSE->getName()];
  if (!Variable)
    return LogErrorV("Unknown variable name");

  Builder->CreateStore(Val, Variable);
  return Val;
}
...
```

변수를 찾았으면 대입은 간단하다. 오른쪽 항 값으로 코드를 생성한 뒤 현재 왼쪽 변수의 주소에다가 넣으면 된다.

# 7.7. 사용자 정의 지역 변수

대입을 만들긴 했는데, 변수도 가능하게 하려면 또다시 렉서, 파서, AST, codegen을 확장해줘야된다.

토큰 생성은 많이 해봤으니까 AST부터,

```cpp
/// VarExprAST - Expression class for var/in
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
             std::unique_ptr<ExprAST> Body)
    : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  Value *codegen() override;
};
```

var/in은 여러 이름을 한번에 정의할 수 있어서, 각 이름이 optional하게 초기값을 가질 수 있다. 그걸 VarNames에 저장해서 꺼내쓸 수 있도록 한다.

파서는 아래와 같다.

```cpp
/// varexpr ::= 'var' identifier ('=' expression)?
//                    (',' identifier ('=' expression)?)* 'in' expression
static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken();  // eat the var.

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  // At least one variable name is required.
  if (CurTok != tok_identifier)
    return LogError("expected identifier after var");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken();  // eat identifier.

    // Read the optional initializer.
    std::unique_ptr<ExprAST> Init;
    if (CurTok == '=') {
      getNextToken(); // eat the '='.

      Init = ParseExpression();
      if (!Init) return nullptr;
    }

    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    // End of var list, exit loop.
    if (CurTok != ',') break;
    getNextToken(); // eat the ','.

    if (CurTok != tok_identifier)
      return LogError("expected identifier list after var");
  }
```

먼저 identifier/expression 쌍을 보고 지역 VarNames 벡터에 다 push_back한다. 

```cpp
Value *VarExprAST::codegen() {
  std::vector<AllocaInst *> OldBindings;

  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    // Emit the initializer before adding the variable to scope, this prevents
    // the initializer from referencing the variable itself, and permits stuff
    // like this:
    //  var a = 1 in
    //    var a = a in ...   # refers to outer 'a'.
    Value *InitVal;
    if (Init) {
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
    } else { // If not specified, use 0.0.
      InitVal = ConstantFP::get(*TheContext, APFloat(0.0));
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    // Remember the old variable binding so that we can restore the binding when
    // we unrecurse.
    OldBindings.push_back(NamedValues[VarName]);

    // Remember this binding.
    NamedValues[VarName] = Alloca;
  }
```

codegen도 값을 하나하나 순회하면서 처리한다. 지역변수를 처리하는 것이다 보니까 shadowing을 막기위해 OldBindings에 값을 임시 저장해둔다. 눈여겨볼 점은 이제 변수가 메모리 위치값을 가리키기 때문에 OldBindgs도 AllocaInst*가 된 것이다.

본문이 다 완성되고 나서 oldbindings에 넣어뒀던 것들을 다시 복구해주면 끝난다.

만들어진 var in은 이렇게 쓴다.

```llvm
var a = 1, b = 2 in
  a + b
```

a,b라는 지역변수를 만들어서 각각 값으로 초기화를 하고, 본문을 실행한다. 본문이 끝나면 이 코드 밖에서 a,b는 사라진다.
