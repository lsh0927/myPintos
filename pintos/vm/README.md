## Keyword Summary

### I. Virtual Memory

> **프로세스가 실제 물리 메모리(RAM)에 직접 접근하지 않고, 운영체제가 제공하는 추상화된 주소 공간을 사용하는 메모리 관리 기법**  
> Mapping? : 프로세스는 자신만의 독립된 메모리 공간을 가지는 것처럼 보이며, OS가 이를 물리 메모리와 mapping을 진행

#### 1. Virtual Memory의 필요성

- Protection
	- 프로세스 간 메모리 침범을 방지
	- 한 프로세스가 다른 프로세스의 메모릴 접근하면 운영체제가 이를 차단
- Abstraction
	- 각 프로세스는 연속적인 큰 메모리를 독립적으로 가진 것처럼 보임
	- 실제 물리 메모리는 파편화되어 있어도 문제 없음
- 효율적 메모리 사용
	- 모든 프로그램을 동시에 메모리에 올릴 필요 없음
	- 요구 페이징(Demand Paging)으로 필요한 페이지만 메모리에 적재

#### 2. 구현 방식
- Paging
	- 가상 주소 공간을 고정 크기의 Page로 나누고, 물리 메모리를 같은 크기의 Frame으로 나눔
	- 가상 주소 → 물리 주소 변환은 **페이지 테이블**로 관리
- Segmentation
	- 코드 / 데이터 / 스택과 같은 의미 있는 단위로 나누어 관리
	- 현재는 페이징과 혼합된 방식으로 사용
- Page Fault
	- 필요한 페이지가 메모리에 없을 때 발생
	- OS가 디스크에서 해당 페이지를 로드

#### 3. PintOS와의 연관성
- 페이지 테이블 관리 시점
	- 각 프로세스마다 독립적인 페이지 테이블 존재
	- 가상 주소와 물리 주소 매핑을 관리
- Lazy Loading
	- 실행 시 전체 프로그램을 메모리에 적재하지 않음
	- 접근 시 필요한 페이지만 디스크에서 불러옴
- Page Fault 처리
	- 잘못된 접근이면 Segmentaation Fault
	- 올바른 접근인데 페이지가 없는 경우 디스크에서 로드
- SWAP
	- 물리 메모리가 부족할 때 일부 페이지를 디스크로 내본내고 필요 시 다시 가져옴

### II. Page Table

> **가상주소와 물리주소를 매핑하기 위한 자료구조**  
> CPU는 프로그램이 사용하는 가상 주소를 직접 물리주소로 변환하지 못하기에, OS가 관리하는 페이지 테이블을 통해 변환 수행

#### 1. 동작 원리
- Page Number / Page Offset
	- Page Number : 페이지 테이블 인덱스로 사용
	- Page Offset : 페이지 내부에서의 위치
- CPU는 Memory Management Unit(MMU)를 통해 변환을 수행
	- 가상 페이지 번호 → 페이지 테이블 참조 → 물리 프레임 번호(PFN)으로 변환
	- 최종 주소 = PFN + Offset

#### 2. 주요 엔트리 = PTE, Page Table Entry
- Present bit: 페이지가 메모리에 존재하는지 여부
- Frame number: 물리 메모리 프레임의 시작 주소
- Protection bits: 읽기 / 쓰기 / 실행 권한
- Dirty bit: 페이지가 수정되었는지 여부
- Accessed bit: 최근에 참조된 여부

#### 3. 구조
- 단일 레벨 페이지 테이블 : Single Level Page Table
	- 단순히 배열 형태
	- 주소 공간이 작을 때 사용
- 다단계 페이지 테이블 : Multi Level Page Table
	- 메모리 낭비를 줄이기 위해 트리 구조 사용
	- x86-32 : 2단계 페이지 테이블 = Page Directory + Page Table
	- x86-64 : 최대 4단계 = Page Map Level 4 → Page Directory Pointer Table → Page Directory → Page Table
- TLB : Translation Lookaside Buffer
	- 페이지 테이블 접근 속도를 높이기 위해, CPU 내부에 최근 주소 변환 결과를 캐싱

#### 4. PintOS와의 연관성
- 가상 주소와 물리 주소 매핑 관리
	- 각 프로세스는 독립적인 페이지 테이블을 가짐
- Lazy Loading
	- 실행 파일을 실행할 때 모든 페이지를 한 번에 로드하지 않고, 접근할 때 페이지 폴트 발생 시 로드
- 스왑(Swap) 지원
	- 물리 메모리가 부족하면 페이지를 디스크로 내보내고, 필요할 때 다시 불러오기
- 보호 기법
	- 잘못된 접근은 페이지 폴트 → 프로세스 종료 (세그멘테이션 폴트와 유사)

### III. Translation Lookaside Buffer, TLB

> **최근 사용된 주소 → 물리 주소 변환 정보를 저장하는 캐시**  
> CPU 내부의 Memory Management Unit(MMU)에 존재, 페이지 테이블 접근 속도를 줄이기 위한 특수 캐시

#### 1. 필요한 이유
- 가상 메모리에서는 가상 주소 → 페이지 테이블 → 물리 주소 변환이 필요
- 다단계 페이지 테이블 구조에서는 변환 시 메모리 접근이 발생
- 변환 결과를 TLB에 저장, 메모리 접근 시간을 획기적으로 줄임

#### 2. 동작 원리
- CPU가 가상 주소를 생성
- MMU가 해당 가상 페이지 번호를 TLB에서 검색
	- TLB Hit : 변환 정보가 있으면 즉시 물리 주소 반환
	- TLB Miss : 변환 정보가 없으면 페이지 테이블을 참조하여 변환, 그리고 결과를 TLB에 저장
- 변환된 물리 주소를 사용해서 실제 메모리에 접근

#### 3. 특징
- 작고 빠른 캐시 : 일반적으로 수십 ~ 수백 개의 엔트리
- Associativity : Direct-mapped, Set-associative, Fully-associative 구조 사용
- TLB Miss Handling
	- 하드웨어가 직접 처리 : x86
	- OS가 처리 : MIPS 등
- Context Switch 문제
	- OS는 직접 TLB에 접근하지 못하고, 페이지 테이블을 관리하는 방식으로 간점적으로 영향
	- TLB Miss가 많아지면 성능 저하 발생 → TLB Locality 최적화가 중요
	- 현대 CPU는 Instruction TLB(ITLB)와 Data TLB(DTLB)를 분리하여 성능을 향상

#### 4. PintOS와의 연관성
- PintOS는 x86 32비트 환경에서 동작하는데, CPU의 하드웨어 TLB를 직접 다루지 않음
- 하지만 페이지 폴트(Page Fault) 처리 시, TLB 갱신이 암묵적으로 발생
	- CPU가 TLB miss → 페이지 테이블 참조 → 잘못된 접근 시 페이지 폴트 발생
	- PintOS에서 페이지 폴트 핸들러를 구현하면, 올바른 페이지 매핑을 생성하고, 이후 CPU가 이를 TLB에 반영
- 즉, PintOS에서는 TLB를 직접 제어하지 않고, 페이지 테이블 관리와 페이지 폴트 처리로 간접적으로 TLB 동작을 경험할 수 있음