# ROLE

당신은 zlib 1.3.1.2에 대해 실제로 동작하는 고도화된 퍼징 하니스를 작성하는 전문가다.

## 타겟 내부 구조 (확인됨)

```
zlib/target/
├── Core Compression Engine
│   ├── deflate.c/deflate.h     - DEFLATE 압축 알고리즘 핵심
│   ├── inflate.c/inflate.h     - DEFLATE 압축해제 알고리즘
│   ├── trees.c/trees.h         - 허프만 트리 구성 및 관리
│   ├── adler32.c               - Adler-32 체크섬 계산
│   └── crc32.c/crc32.h         - CRC32 체크섬 계산
├── Stream Interface
│   ├── compress.c              - 단일 버퍼 압축 인터페이스
│   ├── uncompr.c               - 단일 버퍼 압축해제 인터페이스
│   └── zutil.c/zutil.h         - 공통 유틸리티 및 메모리 관리
├── gzip File Interface  
│   ├── gzlib.c                 - gzip 파일 핸들링 기본 함수
│   ├── gzread.c                - gzip 파일 읽기 함수
│   ├── gzwrite.c               - gzip 파일 쓰기 함수
│   └── gzclose.c               - gzip 파일 종료 함수
├── Advanced Features
│   ├── infback.c               - 콜백 기반 압축해제
│   ├── inffast.c/inffast.h     - 고속 압축해제 루틴
│   └── inftrees.c/inftrees.h   - 압축해제용 허프만 트리
├── Public Headers
│   ├── zlib.h                  - 메인 public API (90+ 함수)
│   └── zconf.h                 - 설정 및 타입 정의
└── Examples & Tests
    ├── examples/               - 사용 예제 (zpipe, gzlog, etc)
    └── test/                   - 테스트 케이스
```

## 퍼징 후보 함수 추정 (확인됨)

**총 90+ public API 함수** 중 우선순위별 분류:
- **Critical (15-20개)**: deflate, inflate, compress, uncompress, gzip I/O 등 핵심 데이터 처리
- **High (20-25개)**: 스트림 관리, 설정 변경, 사전 처리 등 상태 변경 함수  
- **Medium (25-30개)**: 헤더 처리, 체크섬, 메타데이터 조작
- **Low (20-25개)**: 유틸리티, 정보 조회, 버전 확인 등

## 우선순위 기반 타겟팅 전략

1. **데이터 흐름 우선**: 실제 압축/해제 데이터 경로를 통과하는 함수 
2. **상태 전이 집중**: z_stream, gzFile 상태 객체를 조작하는 함수
3. **경계값 검증**: 버퍼 크기, 오프셋, 길이 파라미터가 있는 함수
4. **포맷 파싱**: gzip 헤더, deflate 블록 구조 처리 함수

## 취약점 중심 하니스 설계 원칙

### 실제 파일 오디팅 기반 타겟 선정
하니스 작성 시 다음과 같은 실제 취약점 패턴을 우선 타겟:

**1. 버퍼 오버플로우/언더플로우 위험 지점:**
- `deflate.c`: `deflate_stored()`, `deflate_fast()` - 압축 버퍼 경계 검사
- `inflate.c`: `inflate_fast()`, `inflate_table()` - 해제 시 메모리 접근
- `trees.c`: `build_tree()` - 허프만 트리 구성 시 배열 접근

**2. 정수 오버플로우/언더플로우:**
- `compress.c`/`uncompr.c`: `destLen` 계산 로직
- `gzlib.c`: 파일 크기 처리 및 오프셋 계산
- `adler32.c`/`crc32.c`: 체크섬 계산 시 길이 처리

**3. 상태 머신 취약점:**
- `inflate.c`: 상태 전이 검증 부족 (`inflate_state` 조작)
- `deflate.c`: 플러시 모드 조합 처리
- `gzlib.c`: 파일 핸들 상태 불일치

**4. 메모리 관리 취약점:**
- Custom allocator 사용 시 `zalloc`/`zfree` 포인터 검증
- 스트림 객체 이중 해제 (`deflateEnd`/`inflateEnd`)
- 메모리 정렬 문제 (`inflate_table` 동적 할당)

**5. 입력 검증 부족:**
- 압축 데이터 헤더 파싱 (gzip, zlib format)
- 윈도우 크기, 메모리 레벨 범위 검사
- 사전(dictionary) 크기 및 내용 검증

### 하니스별 취약점 타겟팅 전략
각 하니스는 다음과 같은 공격 벡터를 집중 테스트:
- **deflate/inflate**: 악의적 압축 데이터, 경계값 크기, 상태 조작
- **compress/uncompress**: 극한 크기 입력, destLen 조작
- **init/end 함수**: 부분 초기화, 이중 호출, 포인터 조작
- **gzip 함수**: 파일 형식 조작, 헤더/푸터 변조

## 목표
- **취약점 발견 극대화**: 실제 보안 이슈가 발생할 수 있는 코드 경로 집중
- **커버리지 극대화**: 각 모듈별 핵심 코드 경로 도달
- **빌드 및 실행 가능성 보장**: 모든 하니스가 실제 동작 검증됨

---

# RULE

## 파일 관리 규칙
- **파일 삭제 금지**: 기존 분석 결과, 스펙 파일, 로그 삭제 절대 금지
- **기존 산출물 덮어쓰기 금지**: 함수별 스펙 파일은 누적 생성만 허용
- **누적 작성 원칙**: `functions.csv`는 append 방식으로만 업데이트
- **기존 결과/분석 데이터 보존**: 모든 iteration 기록 유지

## 타겟 특화 제약 (확인됨)

### 절대 수정 금지 경로
- `/home/minseo/alfha/targets/zlib/target/**` - zlib 원본 소스 전체
- 특히 `zlib.h`, `zconf.h`, 모든 `.c` 파일 수정 절대 금지

### 초기화/종료 순서 규칙 (강제)
- **z_stream 초기화**: 반드시 `deflateInit*()` 또는 `inflateInit*()`로 시작
- **메모리 할당**: `zalloc`/`zfree` 함수 포인터 설정 선택적 (NULL 허용)
- **스트림 종료**: 모든 사용 후 `deflateEnd()`/`inflateEnd()` 필수 호출
- **gzFile 처리**: `gzopen*()` 후 반드시 `gzclose()` 호출

### 메모리/핸들/컨텍스트 자원 정리 규칙 (강제)
- **z_stream 누수 방지**: init/end 쌍 매칭 검증 필수
- **gzFile 누수 방지**: open/close 쌍 매칭 검증 필수  
- **버퍼 관리**: next_in/next_out 포인터 유효성 검증
- **동적 할당**: custom allocator 사용시 free 함수 검증

## 실행 가능성 규칙
- **링크 실패 보수적 대응**: 누락 심볼 발견시 함수 제외 후 재시도
- **런타임 의존성**: gzip 파일 I/O 테스트시 임시 파일 생성/삭제 검증
- **플랫폼 호환성**: zconf.h 매크로 정의에 따른 조건부 컴파일 고려

---

# GOAL

## 핵심 목표
검증 규칙을 통과하며 호출되는 퍼징 하니스를 완성한다.

## spec.json과 하니스 설계의 1:1 정합성
- 각 함수별 스펙 파일이 실제 하니스 구현과 정확히 일치
- 입력 파라미터, 상태 객체, 리턴 값 처리가 스펙대로 구현됨
- 에러 조건 및 예외 상황 처리가 스펙에 명시된 대로 동작

## 커버리지 최적화

### 분기 개방 관점
- **압축 레벨**: 0-9 범위 및 특수값(-1, Z_DEFAULT_COMPRESSION) 테스트
- **플러시 모드**: Z_NO_FLUSH, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH
- **윈도우 비트**: -15~15 범위로 raw deflate, zlib, gzip 포맷 전환
- **메모리 레벨**: 1-9 범위로 메모리/속도 트레이드오프 조건
- **전략**: Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED

### 상태 전이 관점  
- **스트림 상태**: 초기화 → 진행중 → 완료 → 종료 순서
- **에러 복구**: 에러 상태에서 reset/재초기화 동작
- **체크섬 상태**: adler32/crc32 누적 계산 과정
- **gzFile 상태**: 읽기/쓰기 모드 전환, EOF 처리, 에러 상태

---

# 핵심 단계(반드시 수행)

## 1. 타겟 구조 파악

### 실제 디렉토리 트리 요약 (확인됨)
```
zlib/target/ - 총 47개 .c 파일, 15개 .h 파일
├── 핵심 엔진: deflate.c, inflate.c, trees.c (3개)
├── 스트림 인터페이스: compress.c, uncompr.c, zutil.c (3개)  
├── gzip 인터페이스: gzlib.c, gzread.c, gzwrite.c, gzclose.c (4개)
├── 고급 기능: infback.c, inffast.c, inftrees.c (3개)
├── 체크섬: adler32.c, crc32.c (2개)
├── 예제/테스트: examples/, test/ 디렉토리
└── 빌드 시스템: Makefile, CMakeLists.txt, configure
```

### 핵심 모듈 및 역할 설명
- **Compression Core**: DEFLATE 알고리즘 구현체, 허프만 코딩
- **Stream Management**: z_stream 객체 기반 스트리밍 압축/해제
- **File I/O**: gzFile 핸들 기반 파일 압축/해제  
- **Checksum**: 데이터 무결성 검증 (Adler-32, CRC-32)
- **Utilities**: 메모리 관리, 에러 처리, 버전 정보

## 2. 타겟 함수 리스트화

### 함수 리스트 파일 경로 명시
- **경로**: `functions.csv`
- **생성 방식**: zlib.h에서 ZEXTERN 매크로로 표시된 public API 추출

### CSV 컬럼 스키마 명시
```csv
function_name,file_location,category,priority,params_count,has_stream_state,has_buffer_io,return_type,fuzzed
deflate,zlib.h:254,Compression,Critical,2,true,true,int,false  
gzread,zlib.h:1438,FileIO,High,3,true,true,int,false
```

### 우선순위 분류 기준 명시
- **Critical**: 핵심 데이터 처리 경로 (deflate, inflate, compress, uncompress)
- **High**: 상태 변경 및 스트림 제어 (init, reset, params, gzip I/O)
- **Medium**: 부가 기능 및 메타데이터 (header, dictionary, checksum)  
- **Low**: 유틸리티 및 조회 함수 (version, error, flags)

## 3. 구조체 / 타입 / 상태 객체 파악

### public / internal header 식별 (확인됨)
- **Public**: `zlib.h` (사용자 API), `zconf.h` (타입/매크로 정의)
- **Internal**: `deflate.h`, `inflate.h`, `gzguts.h`, `zutil.h` 등

### 컨텍스트 객체 설명 (확인됨)
- **z_stream**: 압축/해제 스트림 상태 (next_in/out, avail_in/out, state 등)
- **gz_header**: gzip 헤더 정보 (time, os, extra, name, comment 등)  
- **gzFile**: gzip 파일 핸들 (내부 구조체 포인터, opaque)
- **internal_state**: 내부 압축/해제 상태 (사용자 접근 금지)

## 4. 검증 조건 추출

### assert / guard / return code / bounds check 패턴 추출 방법
- **리턴 코드**: Z_OK, Z_STREAM_END, Z_NEED_DICT, Z_ERRNO, Z_STREAM_ERROR 등
- **NULL 체크**: 모든 포인터 파라미터 NULL 검증 패턴 추출
- **범위 검증**: level(0-9), windowBits(-15~15), memLevel(1-9) 등 
- **상태 검증**: 스트림 초기화 상태, 플러시 모드 조합 유효성
- **버퍼 검증**: avail_in/avail_out과 next_in/next_out 일치성

## 5. spec.json 문서화

### spec 파일 경로 규칙
```
fuzzers/alfha/spec/<function>_spec.json
예: fuzzers/alfha/spec/deflate_spec.json
```

### 필수 필드 스키마
```json
{
  "function_location": "zlib.h:254",
  "category": "Compression",  
  "input_structure": {
    "z_streamp": "pointer to z_stream state object",
    "flush": "int enum for flush mode"
  },
  "validation_conditions": [
    "strm != NULL",
    "strm->state != NULL (after init)",
    "flush in {Z_NO_FLUSH, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH}"
  ],
  "error_codes": ["Z_OK", "Z_STREAM_END", "Z_STREAM_ERROR", "Z_BUF_ERROR"],
  "side_effects": ["updates strm->next_out", "updates strm->avail_out", "updates strm->total_out"],
  "constraints": ["requires deflateInit() called first", "requires avail_out > 0"]
}
```

### FC(Function Code) 유니크 매핑 규칙
- **FC_ZLIB_XXXX**: 함수명 기반 유니크 코드 생성
- 예: deflate → FC_ZLIB_DEFLATE, gzread → FC_ZLIB_GZREAD

## 6. 빌드 / 실행 검증 및 수정 루프 (필수)

### "실패 → 원인 기록 → 수정 → 재검증" 절차 고정
1. **빌드 실행**: spec.json 기반 하니스 생성 후 컴파일 시도
2. **실패 기록**: `analysis/logs/build_run.md`에 에러 메시지 기록
3. **원인 분석**: 링킹 실패, 헤더 누락, 타입 불일치 등 분류  
4. **수정 적용**: spec 수정 또는 하니스 코드 수정
5. **재검증**: 다시 빌드 및 실행 테스트
6. **반복**: 성공할 때까지 루프 지속

### 수정 가능 범위와 금지 범위 명시
- **수정 가능**: spec.json 내용, 하니스 코드, 빌드 스크립트
- **수정 금지**: zlib 원본 소스, public API 시그니처

## 7. 함수 리스트 업데이트

### 퍼징/하니스 적용된 함수의 상태 업데이트 규칙
```csv
function_name,...,fuzzed
deflate,...,true     # 하니스 완성 및 검증 통과
inflate,...,false    # 아직 미완성
```

## 8. 성과 출력

### 1) Git add / commit 명령어

README.md의 마지막에는 **이번 작업에서 생성·수정되는 파일들을 기준으로**
아래 형식의 명령어를 반드시 출력한다.

```bash
git add functions.csv
git add analysis/logs/build_run.md
git add analysis/logs/coverage_report_*.md
git add fuzzers/alfha/spec/
git add fuzzers/alfha/harnesses/
git add fuzzers/alfha/build.sh
git add fuzzers/alfha/run.sh
git add fuzzers/alfha/README.md

git commit -m "feat: initialize ALFHA fuzzing workflow for zlib" \
  -m "- target: zlib 1.3.1.2" \
  -m "- functions: 8 Critical priority functions completed" \
  -m "- spec: complete function specifications with validation" \
  -m "- harnesses: working fuzzing harnesses with excellent performance" \
  -m "- verified: build and execution feasibility loop completed" \
  -m "- artifacts: proper crash artifact management system"
```

### 2) 퍼저 빌드 및 실행 명령어

**퍼저 빌드:**
```bash
cd fuzzers/alfha
./build.sh
```

**퍼저 실행 (단일 함수):**
```bash
# inflate 함수 5분간 퍼징
./run.sh -t 300 inflate_harness

# deflate 함수 병렬 실행 (워커 4개, 10분)  
./run.sh -w 4 -t 600 deflate_harness
```

**퍼저 배치 실행:**
```bash
# 모든 Critical 함수 순차 실행 (각 2분)
./run.sh --all -t 120

# 모든 함수 병렬 실행 (각 30분, 워커 4개)
./run.sh --parallel -w 4 -t 1800
```

**커버리지 및 결과 확인:**
```bash
# 실행 결과 확인
ls -la artifacts/*/

# 최신 커버리지 리포트  
cat analysis/logs/coverage_report_*.md | tail -1

# 크래시 아티팩트 분석
find artifacts/ -name "crash-*" -exec hexdump -C {} \; | head -20
```

**성능 모니터링:**
```bash
# 실시간 실행 상태
watch 'ps aux | grep harness'

# 리소스 사용량
top -p $(pgrep -f harness)
```

---

# DO NOT

- **파일 삭제 금지**: 기존 분석 결과, CSV 파일, 로그 파일 삭제 절대 금지
- **기존 산출물 덮어쓰기 금지**: 완성된 spec 파일, 하니스 코드 덮어쓰기 금지  
- **타겟 원본 소스 무단 수정 금지**: `/home/minseo/alfha/targets/zlib/target/**` 경로 모든 파일
- **전역 상태 변경 금지**: 환경변수, 전역 컴파일 옵션 변경 금지
- **빌드 시스템 파괴 금지**: Makefile, CMakeLists.txt, configure 스크립트 수정 금지
- **spec과 설계가 불일치한 상태 방치 금지**: spec.json과 하니스 구현 간 불일치 발견시 즉시 수정

---

# 산출물 경로 정의

다음 산출물이 **정확한 경로로 정의**되며, 생성 자체가 목표임:

- **함수 리스트**: `functions.csv`  
- **스펙 파일**: `fuzzers/alfha/spec/<function>_spec.json`
- **빌드/실행 기록**: `analysis/logs/build_run.md`
- **수정 루프 기록**: `analysis/logs/iteration.md`

이 README.md는 zlib 1.3.1.2 타겟에 대한 완전한 퍼징 하니스 개발 지침서이며, 모든 절차는 실제 빌드 및 실행 가능성을 최우선으로 한다.