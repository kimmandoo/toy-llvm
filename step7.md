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

이렇게 승격시키는 작업이 직접 SSA를 처리하는 것보다는 빠르다. mem2reg에 흔한 케이스를 처리하기 위한 특수 케이스 들이 내장되어있기 때문에.

# 7.4. Kaleidoscope의 변경 가능한 변수


