https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl04.html

Jit이랑 Optimizer를 구현하는 장이다.

코드를 IR까지 변환했는데, 이 IR을 그대로 받아들여서 실행하는 건 런타임때 손해를 보는 것일 수 있다. 예를들면 1+2+'x'가 들어오면, 컴파일때 이미 3+'x'가 될 것이라는 걸 알 수 있는데 이런 작업을 optimizer가 해준다.

jit은 이렇게 최종적으로 만들어진 ir코드를 기계어로 바꿔서 실행할 수 있도록 해주는 도우미다.

JIT(just in time) compile이 뭐냐면, 프로그램을 실행하는 시점에서 필요한 부분을 즉석으로 컴파일하는 방식을 말한다.

# 4.2. Trivial Constant Folding

IRBuilder는 간단한 코드를 컴파일할 때 명확한 최적화를 자동으로 해준다.

`def test(x) 1+2+x;` 같은 입력이 들어가면, 출력은 아래와 같다.

```ir
define double @test(double %x) {
entry:
        %addtmp = fadd double 3.000000e+00, %x
        ret double %addtmp
}
```
좀 이상하지않나? 원래 같으면 1+2과정이 보여야할 것 같은데?

정답은 상수 계산을 미리 접어주는 `상수 접기(constant folding)`에 있다. LLVM IR을 생성하는 모든 호출은 LLVM IRBuilder를 거치기 때문에 명령을 만들 때마다 상수 접기 기회가 있는지 확인한다. 만약 가능하다면 실제 instruction을 만들지 않고 계산된 constant를 반환해준다.

근데 IRBuilder의 이런 최적화 과정은 IR 생성 시점에만 수행된다.

`ready> def test(x) (1+2+x)*(x+(1+2));` 이래버리면

```ir
define double @test(double %x) {
entry:
        %addtmp = fadd double 3.000000e+00, %x
        %addtmp1 = fadd double %x, 3.000000e+00
        %multmp = fmul double %addtmp, %addtmp1
        ret double %multmp
}
```
이렇게 나오는데, 사실 tmp1이랑 tmp는 동일하기 때문에 tmp * tmp를 기대했을 수 도 있다. 그런데 irbuilder가 이걸 알아서 처리해주진 않는다.

이 문제를 해결하려면 두 가지 변환이 필요하다.

첫 번째는 expression reassociation, 식의 재구성이다. 형태를 똑같이 만들어서 처리하는 방법이다.
두 번째는 Common Subexpression Elimination(CSE)라고 부르는 공통 부분식을 제거해서 중복된 add instruction을 삭제하는 방법이다.

다행히 LLVM은 이러한 최적화를 pass 형태로 다양하게 제공 중이다.

수학적으로 같지만 ir형태가 다른걸 재구성해서 같은 ir형태로 irbuilder가 인식하도록 바꿔주는 작업이라고 할 수 있다.

# 4.3. LLVM Optimization Passes

LLVM 최적화는 “패스” 단위로 생각하면 된다.

쉽게 말하면 패스는 IR을 입력으로 받아 분석하거나 수정해서 더 나은 IR로 바꿔주는 작업단위다.

LLVM은 컴파일러 구현자가 어떤 최적화를 사용할지, 어떤 순서로 사용할지, 어떤 상황에서 사용할지를 완전히 결정할 수 있게 한다.

whole module pass, per-function pass 등을 지원하고 있다.

지금 kaleidoscope의 실행방식은 repl처럼 동작한다. 한줄 입력하면 바로 실행하는 구조기 때문에 실행할 전체 프로그램을 다 본 다음 최적화하기 어렵다. 그래서 이번엔 함수단위 pass를 사용하게 된다.

일반적인 c,cpp 같이 정적컴파일러였으면 모듈 단위 최적화가 일어났을 것이다.

transform pass와 analysis pass가 분리되어있는 것도 인식하고 있어야된다.

transform pass와 analysis pass 차이도 중요합니다.

예를 들어 어떤 최적화가 루프를 바꾸고 싶다면 먼저 “어디가 루프인지” 알아야 하고, 이 정보를 계산하는 것이 analysis pass다.

그 정보를 바탕으로 실제 IR을 바꾸는 것이 transform pass다.

per-function 최적화를 적용하려면 실행할 LLVM 최적화들을 담고 관리할 FunctionPassManager를 설정해야 한다.

그 다음 실행할 최적화들을 추가하면 된다. 최적화하고 싶은 module마다 새로운 FunctionPassManager가 필요하기 떄문에 이전 장에서 만든 InitializeModule() 함수에 내용을 추가할 것이다.

```cpp
void InitializeModuleAndManagers(void) {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("KaleidoscopeJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  // Create new pass and analysis managers.
  TheFPM = std::make_unique<FunctionPassManager>();

  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();

  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                    /*DebugLogging*/ true);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());
  ...
```

전역 module인 TheModule과 FunctionPassManager를 초기화한 뒤, 프레임워크의 다른 부분들도 초기화해야 한다.

네 개의 AnalysisManager는 IR 계층 구조의 네 가지 레벨에서 실행되는 analysis pass를 추가할 수 있게 해주고, 패스 사이에서 일어나는 동작을 커스터마이징하기 위해 PassInstrumentationCallbacks와 StandardInstrumentations는 pass instrumentation framework에 필요하다.

```text
ModuleAnalysisManager - 모듈 전체 분석 담당
FunctionAnalysisManager -  함수 단위 분석 담당
LoopAnalysisManager - 루프 단위 분석 담당
CGSCCAnalysisManager - Call Graph SCC 단위 분석 담당
```
CGSCC는 Call Graph Strongly Connected Component의 약자로 호출 그래프에서 재귀처리된 걸 분석할 때 쓰는 단위다.

JIT도 살짝 나와서 첨언하면, `TheModule->setDataLayout(TheJIT->getDataLayout())`에서 DataLayout은 target machine에서 타입, 포인터, 정렬, 메모리 배치가 어떻게 되는지를 나타낸다.

JIT는 현재 실행 중인 플랫폼에 맞게 기계어를 만들어야 하므로, module도 JIT의 DataLayout을 따라야 한다...

이 manager들이 설정되면, addPass 호출을 사용해서 여러 LLVM transform pass를 추가해준다.

```cpp
// Add transform passes.
// Do simple "peephole" optimizations and bit-twiddling optzns.
TheFPM->addPass(InstCombinePass());
// Reassociate expressions.
TheFPM->addPass(ReassociatePass());
// Eliminate Common SubExpressions.
TheFPM->addPass(GVNPass());
// Simplify the control flow graph (deleting unreachable blocks, etc).
TheFPM->addPass(SimplifyCFGPass());
```

위에서 tmp*tmp를 예시로 들었던게 GVNPass다. global value numbering으로 같은 값을 계산하는 instruction을 찾아서 중복 제거한다.

analysis manager 등록하고 나서 transform pass들이 사용하는 analysis pass들을 등록한다.

```cpp
  // Register analysis passes used in these transform passes.
  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}
```

이렇게 만든걸 클라이언트 반환 전 사용하면 된다. codegen쯤에서 처리하는 것이다.

```cpp
if (Value *RetVal = Body->codegen()) {
  // Finish off the function.
  Builder.CreateRet(RetVal);

  // Validate the generated code, checking for consistency.
  verifyFunction(*TheFunction);

  // Optimize the function.
  TheFPM->run(*TheFunction, *TheFAM);

  return TheFunction;
}
```

이걸 하고 나면 아까 addtmp1, addtmp 이렇게 2개로 쪼개졌던 게 addtmp하나의 instruction으로 변해있을 것이다.

실행시점을 좀 더 자세히 볼 필요가 있다.

IR 검증 후 최적화를 수행하는데, 최적화 후 IR 검증을 돌리면 이상한 결과를 내뿜게 될 수 도 있다. 정상적으로 동작하는 IR에다가 최적화를 하는 것이지 줄일 거 다 줄이고 ir 검증하는 건 잘못됐다.

# 4.4. Adding a JIT Compiler

위에서 간단히 얘기했었는데, 좀 더 풀어보겠다.

소스 코드 -> 컴파일 -> 실행 파일 생성 -> 나중에 실행 이 정적 컴파일 흐름이라면, JIT은 LLVM IR 생성 -> 즉석에서 기계어 생성 -> 바로 실행 으로 이뤄진다.

인터프리터지만 기계어를 실행하게 만드는 게 JIT의 개념이다.

```cpp
static std::unique_ptr<KaleidoscopeJIT> TheJIT;
...
int main() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  TheJIT = std::make_unique<KaleidoscopeJIT>();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
```

JIT 전역변수를 선언하고 초기화시켜준다. KaleidoscopeJIT는 아래 링크에 정의돼있다.

https://github.com/llvm/llvm-project/blob/main/llvm/examples/Kaleidoscope/include/KaleidoscopeJIT.h

```cpp
void InitializeModuleAndPassManager(void) {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());
  ...
```

JIT의 DataLayout도 설정해야된다. addModule은 LLVM IR module을 JIT에 추가해서 그 안의 함수들을 실행 가능하게 만드는데 이때 메모리는 ResourceTracker가 관리한다.

InitializeNativeTarget() 계열 함수는 “현재 프로그램이 실행 중인 플랫폼을 타겟으로 삼겠다”는 초기화이다.

window라면 거기에 맞게, linux라면 linux x64에 맞게 해주는 것이다.

이 간단한 API를 사용해서 top-level expression을 파싱하는 코드를 다음처럼 바꿀 수 있다.

```cpp
static ExitOnError ExitOnErr;
...
static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      // Create a ResourceTracker to track JIT'd memory allocated to our
      // anonymous expression -- that way we can free it after executing.
      auto RT = TheJIT->getMainJITDylib().createResourceTracker();

      auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
      InitializeModuleAndPassManager();

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = ExprSymbol.toPtr<double (*)()>();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      ExitOnErr(RT->remove());
    }
``
파싱과 codegen이 성공하면 다음 단계는 top-level expression이 들어 있는 module을 JIT에 추가하는 것이다. 이걸 위해 addModule을 호출하는데, 이건 module 안의 모든 함수에 대한 코드 생성을 트리거한다.

또한 나중에 JIT에서 module을 제거할 수 있도록 ResourceTracker를 받는다.

module이 JIT에 추가되면 더 이상 수정할 수 없다. 그래서 이후 코드를 담기 위해 InitializeModuleAndPassManager()를 호출해서 새로운 module을 연다.

module을 JIT에 추가한 뒤에는 최종 생성된 코드의 포인터를 얻어야 하는데 JIT의 lookup 메서드를 호출해서 수행합니다. 이때 top-level expression 함수의 이름인 __anon_expr를 넘긴다.

그런다음 __anon_expr 함수의 메모리 주소를 얻는다.

top-level expression은 인자를 받지 않고 계산된 double을 반환하는 독립적인 LLVM 함수로 컴파일되고있는 걸 알고 있어야한다.

LLVM JIT 컴파일러는 native platform ABI에 맞춰 동작하기 때문에 결과 포인터를 해당 타입의 함수 포인터로 캐스팅한 뒤 직접 호출할 수 있다.

즉, JIT 컴파일된 코드와 애플리케이션에 정적으로 링크된 native machine code 사이에는 차이가 없다.

마지막으로 top-level expression 재평가를 지원하지 않기 때문에, 실행이 끝나면 해당 module을 JIT에서 제거해서 관련 메모리를 해제한다.

라인별로 정리하면 이렇게 된다.

1. ParseTopLevelExpr()
   사용자가 입력한 식을 익명 함수 AST로 만든다.

2. FnAST->codegen()
   LLVM IR 함수 __anon_expr를 만든다.

3. ResourceTracker 생성
   이 익명 표현식용 JIT 메모리를 추적한다.

4. ThreadSafeModule 생성
   TheModule과 TheContext의 소유권을 JIT로 넘길 준비를 한다.

5. TheJIT->addModule(...)
   module을 JIT에 추가하고 기계어 생성 가능 상태로 만든다.

6. InitializeModuleAndPassManager()
   기존 module은 JIT로 넘겼으므로 새 module을 연다.

7. TheJIT->lookup("__anon_expr")
   JIT 안에서 익명 함수의 주소를 찾는다.

8. 함수 포인터로 캐스팅
   double (*)() 타입으로 바꾼다.

9. FP()
   진짜 native 함수처럼 호출한다.

10. RT->remove()
    익명 표현식 module을 JIT에서 제거한다.

top-level expression을 왜 함수로 감싸는지 의문이 들었었는데, IR이 그냥 표현식 하나만 실행하는 구조가 아니기 때문임을 이해했다.

그러니까. 4 + 5; 이게 아니라

```llvm
define double @__anon_expr() {
entry:
  ret double 9.000000e+00
}
```
이거다. 이 뒤에 JIT이 이 함수를 컴파일해서 호출한다.

하나 중요한건, 익명 표현식 모듈이 사용 이후 사라진다는 것인데 이거 때문에 만약 함수 정의와 일회성 표현식을 같은 module에 넣으면 나중에 함수 호출을 하고 싶어도 이미 모듈이 제거돼서 호출할 수 없게 된다.

그래서 함수정의 모듈과 익명 표현식 모듈을 따로 관리해야된다.

각 함수가 자기 module 안에 존재하게 하려면, 새 module을 열 때마다 이전 함수 선언들을 다시 생성할 수 있는 방법이 필요하다.

```cpp
static std::unique_ptr<KaleidoscopeJIT> TheJIT;

...

Function *getFunction(std::string Name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  // If no existing prototype exists, return null.
  return nullptr;
}

...

Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  Function *CalleeF = getFunction(Callee);

...

Function *FunctionAST::codegen() {
  // Transfer ownership of the prototype to the FunctionProtos map, but keep a
  // reference to it for use below.
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;
```

getFunction()는 먼저 TheModule에서 기존 함수 선언을 찾는다. 찾지 못하면 FunctionProtos에 저장된 prototype을 기반으로 새 선언을 생성한다.

CallExprAST::codegen()에서는 TheModule->getFunction() 호출을 getFunction()으로 바꾸기만 하면 되는데 FunctionAST::codegen()에서는 먼저 FunctionProtos map을 업데이트한 다음, getFunction()을 호출해야 한다.

이렇게 하면 이전에 선언된 어떤 함수에 대해서도 현재 module 안에서 함수 선언을 얻을 수 있다.

코드를 다시 보면, 함수에 대한 가장 최신 prototype을 FunctionProtos에 저장해두는데, 이게 왜 필요할까? 

`def foo(x) x + 1;`이런 함수 정의가 있었고 새 모듈에서 foo(2); 를 컴파일 했다고 쳐보자.

그러면 새 모듈에서는 실제 구현이 현재 모듈안에 없다. 근데 호출 instruction을 만들려면 시그니처에 대한 정보가 있어야하기 때문에 prototype이 필요하고 FunctionProtos가 필요하다.

실제 구현은 다른 module/JIT 안에 있어도 된다. 현재 module은 “이런 함수가 있다”는 선언만 알고 있으면 call instruction을 만들 수 있다.

구현체는 다른 모듈에 있고 함수 선언맨 새 모듈마다 재생성하는 형태다.

```cpp
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      ExitOnErr(TheJIT->addModule(
          ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
      InitializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
     getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}
```

HandleDefinition에서는 새로 정의된 함수를 JIT로 넘기고 새 module을 열기 위해 두 줄을 추가한다. HandleExtern에서는 prototype을 FunctionProtos에 추가하는 한 줄만 있으면 된다.

LLVM 9부터는 중복 symbol이 금지되기 때문에 이부분만 조심하면 되겠다.

```cpp
ready> extern sin(x);
Read extern:
declare double @sin(double)

ready> extern cos(x);
Read extern:
declare double @cos(double)

ready> sin(1.0);
Read top-level expression:
define double @2() {
entry:
  ret double 0x3FEAED548F090CEE
}

Evaluated to 0.841471

ready> def foo(x) sin(x)*sin(x) + cos(x)*cos(x);
Read function definition:
define double @foo(double %x) {
entry:
  %calltmp = call double @sin(double %x)
  %multmp = fmul double %calltmp, %calltmp
  %calltmp2 = call double @cos(double %x)
  %multmp4 = fmul double %calltmp2, %calltmp2
  %addtmp = fadd double %multmp, %multmp4
  ret double %addtmp
}

ready> foo(4.0);
Read top-level expression:
define double @3() {
entry:
  %calltmp = call double @foo(double 4.000000e+00)
  ret double %calltmp
}

Evaluated to 1.000000
```

JIT는 어떻게 sin과 cos를 알고 있을까?

먼저 JIT에 이미 추가된 모든 module을 가장 최신 것부터 가장 오래된 것 순서로 검색합니다. 이렇게 해서 가장 최신 정의를 찾고, 만약 JIT 내부에서 정의를 못찾았으면 프로세스 자체에 대해 sin을 호출하는 방식으로 fallback된다.

여기서 sin은 JIT의 address space 안에 정의되어 있으므로, module 안의 호출을 libm 버전의 sin을 직접 호출하도록 패치할 수 있다.

symbol resolution에 집중해야된다.

JIT 컴파일된 코드 안에 `call double @sin(double 1.0)` 이런게 있으면 JIT이 sin이라는 symbol의 실제 주소를 찾야아한다.

최신 모듈에서 sin을 검색했다가, 없으면 좀 오래된 모듈에서 찾고, 거기도 없으면 현재 프로세스에서 찾는다. 만약 link된 라이브러리가 있으면 거기서 찾게 될 수 도 있을 것이다.

만약 수학으로 인식하고 최적화가 발생하면 함수 호출 자체도 사라질 수 있다!

이게 가능한 이유는 JIT 코드도 결국 같은 프로세스 안에서 실행되기 때문이다.

구조 잡아보면 이렇다.

```text
내 C++ 프로그램
├── Kaleidoscope REPL 코드
├── LLVM JIT 엔진
├── JIT가 만든 기계어
└── libm의 sin/cos 함수
```

symbol resolution 규칙의 즉각적인 장점 중 하나는 C++ 코드를 작성해서 언어를 확장할 수 있다는 것이다.

```cpp
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
```

이런 걸 만들 수도 있겠다. 이걸 `extern putchard(x);`이렇게 선언해주면 호출 시 저 함수가 호출되게 된다.

extern "C"가 중요한 이유는 C++ name mangling을 막기 위해서다.

C++에서는 함수 오버로딩 때문에 컴파일된 symbol 이름이 바뀌는데 그러면 JIT이 putchard라는 이름으로 함수를 찾을 수 없게 된다.

한 가지더 추가하면 windows에서는 dynamic symbol lookup을 하려면 해당 함수가 export되어 있어야 한다. 그래서 #define으로 export 해준 것이다.

## cpp 문법 보기

# 1. `#ifdef`, `#else`, `#endif`

```cpp
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif
```

이건 C++ 코드처럼 보이지만, 정확히는 **전처리기 문법**입니다.

컴파일러가 C++ 코드를 본격적으로 컴파일하기 전에, 전처리기가 먼저 처리합니다.

## `#ifdef`

```cpp
#ifdef _WIN32
```

뜻은 다음과 같습니다.

```text
_WIN32라는 매크로가 정의되어 있으면
아래 코드를 포함해라
```

`_WIN32`는 Windows 환경에서 보통 컴파일러가 자동으로 정의해주는 매크로입니다.

즉 이 코드는:

```text
Windows에서 컴파일 중인가?
```

를 확인하는 코드입니다.

## `#else`

```cpp
#else
```

뜻은 다음과 같습니다.

```text
위 조건이 아니면 이쪽 코드를 사용해라
```

즉 Windows가 아니면 Linux, macOS 같은 환경으로 보고 아래 코드를 사용합니다.

## `#endif`

```cpp
#endif
```

조건부 전처리 블록의 끝입니다.

정리하면:

```cpp
#ifdef _WIN32
// Windows일 때만 들어감
#else
// Windows가 아닐 때 들어감
#endif
```

입니다.

---

# 2. `#define DLLEXPORT ...`

```cpp
#define DLLEXPORT __declspec(dllexport)
```

`#define`은 매크로를 정의합니다.

여기서는 `DLLEXPORT`라는 이름을 만들고 있습니다.

Windows일 때는 이렇게 됩니다.

```cpp
#define DLLEXPORT __declspec(dllexport)
```

그러면 이후 코드에서:

```cpp
extern "C" DLLEXPORT double putchard(double X)
```

라고 쓴 부분이 전처리 후에는 이렇게 바뀝니다.

```cpp
extern "C" __declspec(dllexport) double putchard(double X)
```

반대로 Windows가 아니면:

```cpp
#define DLLEXPORT
```

입니다.

이건 `DLLEXPORT`를 아무것도 아닌 것으로 정의한다는 뜻입니다.

그래서 Linux/macOS에서는:

```cpp
extern "C" DLLEXPORT double putchard(double X)
```

가 전처리 후 이렇게 됩니다.

```cpp
extern "C" double putchard(double X)
```

즉 `DLLEXPORT`가 사라집니다.

---

# 3. `__declspec(dllexport)`

```cpp
__declspec(dllexport)
```

이건 표준 C++ 문법이라기보다는 **MSVC/Windows 계열 확장 문법**입니다.

Windows에서 DLL이나 실행 파일의 symbol을 외부에서 찾을 수 있게 export할 때 사용합니다.

이 코드에서 왜 필요하냐면, LLVM JIT가 `putchard`라는 함수를 이름으로 찾아야 하기 때문입니다.

```kaleidoscope
extern putchard(x);
putchard(120);
```

Kaleidoscope 쪽에서 이렇게 호출하면 JIT는 현재 프로세스 안에서 `putchard`라는 symbol을 찾습니다.

그런데 Windows에서는 함수를 명시적으로 export하지 않으면 동적 symbol lookup에서 못 찾을 수 있습니다.

그래서 Windows에서는:

```cpp
__declspec(dllexport)
```

를 붙입니다.

즉:

```cpp
extern "C" __declspec(dllexport) double putchard(double X)
```

는 대략 이런 뜻입니다.

```text
putchard라는 함수를 C ABI 이름으로 노출하고,
Windows에서 외부 symbol lookup이 가능하도록 export해라.
```

---

# 4. `extern "C"`

```cpp
extern "C" DLLEXPORT double putchard(double X)
```

여기서 가장 중요한 문법이 `extern "C"`입니다.

이건 C++ 함수의 **이름 맹글링(name mangling)**을 막기 위한 문법입니다.

## C++은 함수 이름을 그대로 저장하지 않는다

C++에는 함수 오버로딩이 있습니다.

```cpp
void foo(int x) {}
void foo(double x) {}
void foo(int x, int y) {}
```

C++에서는 같은 이름의 함수가 여러 개 있을 수 있습니다.

그래서 컴파일러는 실제 symbol 이름에 타입 정보를 섞습니다.

예를 들어:

```cpp
double putchard(double)
```

가 실제 바이너리 symbol에서는 이런 식으로 바뀔 수 있습니다.

```text
_Z8putchardd
```

이걸 **name mangling**이라고 합니다.

문제는 LLVM JIT가 찾는 이름은 `putchard`라는 점입니다.

```kaleidoscope
extern putchard(x);
```

라고 선언하면 JIT는 진짜로 `"putchard"`라는 symbol을 찾습니다.

그런데 C++ 컴파일러가 이름을 `_Z8putchardd`로 바꿔버리면 못 찾습니다.

그래서:

```cpp
extern "C"
```

를 붙입니다.

그러면 C++ 컴파일러에게 이렇게 말하는 겁니다.

```text
이 함수는 C 방식 linkage를 사용해라.
함수 이름을 C++ 방식으로 맹글링하지 마라.
symbol 이름을 putchard 그대로 남겨라.
```

결과적으로 JIT가 `putchard`라는 이름으로 함수를 찾을 수 있습니다.

---

# 5. 함수 선언 구조

```cpp
extern "C" DLLEXPORT double putchard(double X)
```

이 줄을 쪼개면 다음과 같습니다.

```cpp
extern "C"     // C linkage 사용
DLLEXPORT      // Windows에서는 export, 그 외에는 빈 매크로
double         // 반환 타입
putchard       // 함수 이름
(double X)     // double 타입 매개변수 X
```

즉 전체 의미는:

```text
double 하나를 받아서 double을 반환하는 putchard 함수다.
다만 symbol 이름은 C 방식으로 노출한다.
Windows에서는 export한다.
```

입니다.

---

# 6. 함수 본문

```cpp
{
  fputc((char)X, stderr);
  return 0;
}
```

함수 본문은 단순합니다.

## `fputc`

```cpp
fputc((char)X, stderr);
```

`fputc`는 C 표준 라이브러리 함수입니다.

문자 하나를 특정 출력 스트림에 씁니다.

형태는 대략 다음과 같습니다.

```cpp
int fputc(int character, FILE* stream);
```

예를 들어:

```cpp
fputc('x', stderr);
```

는 `stderr`에 문자 `x`를 출력합니다.

---

# 7. `(char)X`

```cpp
(char)X
```

이건 C 스타일 타입 캐스팅입니다.

`X`는 `double`입니다.

```cpp
double X
```

그런데 `fputc`는 문자로 출력할 값을 받습니다.

그래서 `double`을 `char`로 변환합니다.

예를 들어 Kaleidoscope에서:

```kaleidoscope
putchard(120);
```

을 호출하면 C++ 함수에는 이렇게 들어옵니다.

```cpp
X = 120.0
```

그 다음:

```cpp
(char)X
```

를 하면 `120.0`이 문자 코드 `120`으로 변환됩니다.

ASCII 코드에서 `120`은 `'x'`입니다.

그래서 화면에 `x`가 출력됩니다.

---

# 8. `stderr`

```cpp
stderr
```

`stderr`는 표준 에러 출력 스트림입니다.

C/C++에는 대표적으로 이런 표준 스트림이 있습니다.

| 이름       | 의미       | 보통 연결되는 곳 |
| -------- | -------- | --------- |
| `stdin`  | 표준 입력    | 키보드       |
| `stdout` | 표준 출력    | 콘솔        |
| `stderr` | 표준 에러 출력 | 콘솔        |

여기서는 `stdout`이 아니라 `stderr`에 출력하고 있습니다.

```cpp
fputc((char)X, stderr);
```

왜 `stderr`를 썼냐면, LLVM 튜토리얼 코드에서 REPL 메시지나 디버그 출력도 보통 `stderr`를 많이 사용하기 때문입니다.

실제 프로그램 출력과 디버그/로그 출력을 분리하려는 의도도 있습니다.

---

# 9. `return 0;`

```cpp
return 0;
```

함수 반환 타입은 `double`입니다.

```cpp
double putchard(double X)
```

그런데 `0`은 int 리터럴입니다.

C++에서는 `int` 값 `0`을 `double`로 암시적 변환할 수 있습니다.

즉 실제 반환은:

```cpp
return 0.0;
```

과 비슷하게 처리됩니다.

이 함수는 출력이 목적입니다. 반환값 자체는 별 의미가 없습니다.

Kaleidoscope 언어는 모든 값이 `double`인 단순한 언어이기 때문에, 외부 함수도 `double`을 반환하게 만든 것입니다.

---

# 10. 왜 굳이 `double`을 받는가?

```cpp
double putchard(double X)
```

문자 하나 출력하는 함수라면 원래는 이렇게 만드는 게 자연스럽습니다.

```cpp
void putchard(char c)
```

또는:

```cpp
int putchar(int c)
```

그런데 튜토리얼에서는 `double`을 사용합니다.

이유는 Kaleidoscope 언어가 이 시점에서는 모든 값을 `double`로 다루기 때문입니다.

예를 들어 Kaleidoscope의 함수들은 기본적으로 이런 형태입니다.

```llvm
double @foo(double %x)
```

그래서 외부 함수도 맞춰줍니다.

```cpp
extern "C" double putchard(double X)
```

즉 이 함수는 C++ 입장에서는 이상해 보이지만, Kaleidoscope와 연결하기 위해 일부러 이렇게 만든 것입니다.

---

# 11. 전체 코드를 전처리 후 모습으로 보면

## Windows일 때

원본:

```cpp
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
```

전처리 후:

```cpp
extern "C" __declspec(dllexport) double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
```

## Linux/macOS일 때

전처리 후:

```cpp
extern "C" double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
```

---

# 최종 정리

이 코드에서 알 수 있는 C++ 문법은 다음입니다.

| 문법                          | 의미                                 |
| --------------------------- | ---------------------------------- |
| `#ifdef`                    | 특정 매크로가 정의되어 있을 때만 코드 포함           |
| `_WIN32`                    | Windows 컴파일 환경에서 주로 정의되는 매크로       |
| `#define`                   | 매크로 정의                             |
| `#else`                     | 조건이 false일 때 사용할 코드                |
| `#endif`                    | 조건부 전처리 종료                         |
| `__declspec(dllexport)`     | Windows에서 symbol을 외부로 export       |
| `extern "C"`                | C linkage 사용, C++ name mangling 방지 |
| `double putchard(double X)` | double을 받고 double을 반환하는 함수         |
| `(char)X`                   | C 스타일 타입 캐스팅                       |
| `fputc`                     | 문자 하나를 출력 스트림에 쓰는 C 함수             |
| `stderr`                    | 표준 에러 출력 스트림                       |
| `return 0`                  | double 반환 함수에서 0을 반환, 암시적으로 0.0 변환 |

---