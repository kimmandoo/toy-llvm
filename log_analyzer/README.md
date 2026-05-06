## android log 분석기

1. file로 로그 읽어옴
2. event별 빈도 측정
3. event level 별 빈도 측정
4. 어떤 error 이벤트가 언제 발생했는 지 기록

우선 싱글 스레드로 만들고, 멀티 스레드로 발전시키기

## trouble shooting

### `I ACE    : text`, `Mams:BackupManager:`와 같이 이상한 포맷이 tag위치에 숨어있음

iss로 스트림 받아서 토큰 끊으면 ACE만 읽힘.

`: `를 기준으로 한 번 끊어서 다시 처리하게

### trim함수 c11에 없음

```cpp
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}
```

만들어서 사용

### `std::istringstream` 제거

iss가 실행 될 때 내부적으로 임시 문자열 복사, 버퍼 초기화가 계속 발생하는데 이걸 수백만 줄 로그에 매번 호출하면 성능 저하 발생...

공백 위치만 따라가면서 직접 문자열 토큰화 적용

target 로그 파일은 약 707만 line의 안드로이드 로그파일

before

```bash
 Performance counter stats for './a.out target.log':

        5833261700      task-clock:u                     #    0.904 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
              7046      page-faults:u                    #    1.208 K/sec                     
       67173384544      instructions:u                   #    2.78  insn per cycle            
                                                  #    0.00  stalled cycles per insn   
       24199969892      cycles:u                         #    4.149 GHz                       
          89101891      stalled-cycles-frontend:u        #    0.37% frontend cycles idle      
          21793948      stalled-cycles-backend:u         #    0.09% backend cycles idle       
       14225179498      branches:u                       #    2.439 G/sec                     
          45729645      branch-misses:u                  #    0.32% of all branches           

       6.452690211 seconds time elapsed

       5.150092000 seconds user
       0.682432000 seconds sys
```

after

```bash
 Performance counter stats for './a.out target.log':

        4164682200      task-clock:u                     #    0.900 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
              7942      page-faults:u                    #    1.907 K/sec                     
       40759051194      instructions:u                   #    2.52  insn per cycle            
                                                  #    0.00  stalled cycles per insn   
       16186377422      cycles:u                         #    3.887 GHz                       
          47124765      stalled-cycles-frontend:u        #    0.29% frontend cycles idle      
          16300362      stalled-cycles-backend:u         #    0.10% backend cycles idle       
        8798683981      branches:u                       #    2.113 G/sec                     
          32728184      branch-misses:u                  #    0.37% of all branches           

       4.627105785 seconds time elapsed

       3.532178000 seconds user
       0.631399000 seconds sys
```

전체 실행 시간 28% 개선
instruction 개수 39% 개선

불필요한 객체 생성이 없어지면서 instruction 개수가 줄어든 것으로 보임

### vector에 문자열 추가하는 대신 offset만 저장해서 처리하기

로그 파일이 커질수록 vector에 저장되는 데이터 또한 엄청 늘어날 것. 메모리 공간 초과가 발생할 수 있기 때문에 개선해보겠음.

```cpp
for (std::streampos offset : result.errorOffsets) {
    originalFile.seekg(offset); // 해당 바이트 위치로 포인터 이동
    std::getline(originalFile, errorLine); // 그 한 줄만 읽기
    outFile << errorLine << "\n";
}
```

streampos를 쓴다. std::streampos는 파일/스트림 내부 위치를 나타내는 타입.

offset으로 에러 읽어올 때는 당연히 다른 stream으로 파일을 열어야됨.

before
```bash
 Performance counter stats for './a.out target.log':

        4098312500      task-clock:u                     #    0.901 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
              7257      page-faults:u                    #    1.771 K/sec                     
       40069985213      instructions:u                   #    2.49  insn per cycle            
                                                  #    0.00  stalled cycles per insn   
       16061188341      cycles:u                         #    3.919 GHz                       
          62378787      stalled-cycles-frontend:u        #    0.39% frontend cycles idle      
          14096448      stalled-cycles-backend:u         #    0.09% backend cycles idle       
        8612037313      branches:u                       #    2.101 G/sec                     
          34402430      branch-misses:u                  #    0.40% of all branches           

       4.550336995 seconds time elapsed

       3.444756000 seconds user
       0.652690000 seconds sys
```
after
```bash
 Performance counter stats for './a.out target.log':

        3376384900      task-clock:u                     #    0.897 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
               974      page-faults:u                    #  288.474 /sec                      
       21758779563      instructions:u                   #    2.31  insn per cycle            
                                                  #    0.01  stalled cycles per insn   
        9401937609      cycles:u                         #    2.785 GHz                       
         211613645      stalled-cycles-frontend:u        #    2.25% frontend cycles idle      
          56140139      stalled-cycles-backend:u         #    0.60% backend cycles idle       
        4657979802      branches:u                       #    1.380 G/sec                     
          39074444      branch-misses:u                  #    0.84% of all branches           

       3.766160090 seconds time elapsed

       2.202098000 seconds user
       1.174452000 seconds sys
```

문자열 복사 저장을 오프셋으로 처리해서 원문 그대로 갖다가 읽음.

page-fault 86%감소 복사, 저장이 안일어나니까 heap 할당도 확 줄어든다.
instruction도 반토막
user타임은 줄고, sys타임은 늘었음, 근데 전체 시간은 줄었음.
-> 문자열 연산 안하니까 앱 내 연산 시간은 확 줄고, 시스템 함수로 offset 처리해야되서 sys시간이 늘어났다. 시스템 콜은 늘었지만 문자열 복사가 없어져서 전체 실행시간이 줄었음.