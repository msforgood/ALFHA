# ALFHA zlib Fuzzing Harnesses

이 디렉토리는 zlib 1.3.1.2 라이브러리의 중요 함수들에 대한 체계적인 퍼징 하니스를 포함합니다.

## 📁 디렉토리 구조

```
fuzzers/alfha/
├── build.sh              # 퍼저 빌드 스크립트
├── run.sh                # 퍼저 실행 스크립트  
├── README.md             # 이 파일
├── harnesses/            # 퍼징 하니스 소스코드
│   ├── *_harness.c      # 각 함수별 하니스 구현
│   └── *_harness        # 컴파일된 실행파일
├── spec/                 # 함수별 상세 스펙
│   └── *_spec.json      # JSON 형태의 함수 명세
├── corpus/               # 퍼징 코퍼스 (자동 생성)
│   └── */               # 함수별 테스트케이스
└── artifacts/            # 크래시/타임아웃 아티팩트
    └── *_YYYYMMDD_HHMMSS/  # 타임스탬프별 분류
```

## 🎯 퍼징 대상 함수 (Critical Priority)

| 함수 | 카테고리 | 설명 | 상태 |
|------|----------|------|------|
| `deflateInit` | Compression | 압축 초기화 | ✅ 완료 |
| `deflate` | Compression | 압축 실행 | ✅ 완료 |
| `deflateEnd` | Compression | 압축 정리 | ✅ 완료 |
| `inflateInit` | Compression | 해제 초기화 | ✅ 완료 |
| `inflate` | Compression | 압축 해제 | ✅ 완료 |
| `inflateEnd` | Compression | 해제 정리 | ✅ 완료 |
| `compress` | Stream | 단순 압축 | ✅ 완료 |
| `uncompress` | Stream | 단순 해제 | ✅ 완료 |

## 🔨 빌드 방법

### 자동 빌드 (권장)
```bash
# 퍼저 빌드 스크립트 실행
cd fuzzers/alfha
./build.sh
```

### 수동 빌드
```bash
# zlib 라이브러리 빌드 (필요시)
cd ../../target
./configure && make

# 개별 하니스 빌드
cd ../fuzzers/alfha/harnesses
clang -fsanitize=fuzzer -I../../../target harness_name.c -L../../../target -lz -o harness_name
```

## 🚀 퍼저 실행 방법

### 기본 사용법

```bash
# 도움말 확인
./run.sh --help

# 사용 가능한 하니스 목록 확인  
./run.sh --list

# 단일 하니스 실행 (60초)
./run.sh inflate_harness

# 특정 시간 동안 실행 (300초 = 5분)
./run.sh -t 300 deflate_harness

# 병렬 워커로 실행
./run.sh -w 4 -t 600 inflate_harness
```

### 배치 실행

```bash
# 모든 하니스 순차 실행 (각각 120초)
./run.sh --all -t 120

# 모든 하니스 병렬 실행 (각각 600초, 워커 4개)  
./run.sh --parallel -w 4 -t 600
```

### 실행 예시

```bash
# 압축 관련 함수만 집중 테스트
./run.sh -t 1800 deflate_harness     # 30분간 deflate 테스트
./run.sh -t 1800 inflate_harness     # 30분간 inflate 테스트

# 전체 함수 장시간 테스트  
./run.sh --all -t 3600               # 각 함수당 1시간씩 순차 실행
```

## 📊 성능 및 커버리지

최신 성능 측정 결과 (60초 실행 기준):

| 함수 | 실행횟수 | 속도 (exec/sec) | 커버리지 | 메모리 | 성능 등급 |
|------|----------|----------------|-----------|---------|-----------|
| **inflate** | 1,297,515 | 21,991 | 28 블록 | 27MB | 🥇 최고 |
| **inflateInit** | 520,158 | 173,386 | 23 블록 | 2,059MB | ⚡ 초고속 |
| **deflate** | 476,476 | 8,075 | 18 블록 | 27MB | 🎯 우수 |
| deflateInit | 175,622 | 2,976 | 16 블록 | 27MB | ✅ 양호 |

- **최고 성능**: inflate, deflate 하니스 (높은 처리량과 커버리지)
- **빠른 탐색**: inflateInit 하니스 (초당 17만회 실행)
- **안정적 동작**: 모든 하니스에서 크래시 없이 60초 실행 완료

## 🔍 아티팩트 관리

### 크래시 아티팩트 위치
```
artifacts/
├── deflate_harness_20231216_140530/
│   ├── crash-abc123...               # 크래시 재현 파일
│   └── timeout-def456...            # 타임아웃 케이스
└── inflate_harness_20231216_141200/
    └── oom-789abc...                # 메모리 부족 케이스  
```

### 아티팩트 분석
```bash
# 크래시 아티팩트 확인
find artifacts/ -name "crash-*" -ls

# 최신 크래시 재현
./harnesses/inflate_harness artifacts/inflate_harness_*/crash-*

# 커버리지 리포트 확인
cat analysis/logs/coverage_report.md
```

## ⚙️ 고급 설정

### 환경 변수
```bash
# 최대 입력 크기 제한
export ASAN_OPTIONS=mmap_limit_mb=2048

# 크래시 시 코어덤프 생성
export ASAN_OPTIONS=abort_on_error=1

# 더 자세한 로깅
export ASAN_OPTIONS=verbosity=1
```

### 커스텀 실행
```bash
# 특정 시드로 실행
./harnesses/deflate_harness -seed=12345 corpus/deflate

# 최대 입력 크기 제한
./harnesses/inflate_harness -max_len=1024 corpus/inflate

# 사전 정의된 입력으로 시작
./harnesses/compress_harness existing_corpus/ new_corpus/
```

## 📈 모니터링 및 분석

### 실시간 모니터링
```bash
# 실행 상태 확인
ps aux | grep harness

# 시스템 리소스 사용량
top -p $(pgrep -f harness)

# 디스크 사용량 확인  
du -sh corpus/ artifacts/
```

### 결과 분석
```bash
# 커버리지 통계
./run.sh -t 300 inflate_harness 2>&1 | grep "cov:"

# 새로운 테스트케이스 발견
find corpus/ -name "*" -newer /tmp/start_time | wc -l

# 크래시 분석
for crash in artifacts/*/crash-*; do
    echo "=== $crash ==="
    hexdump -C "$crash" | head -5
done
```

## 🏗️ 개발 정보

- **타겟**: zlib 1.3.1.2
- **퍼저**: libFuzzer (LLVM)
- **컴파일러**: clang with AddressSanitizer
- **플랫폼**: Linux x86_64
- **개발일**: 2025-12-16

## 📚 참고 자료

- [zlib 공식 문서](https://zlib.net/manual.html)
- [libFuzzer 사용법](https://llvm.org/docs/LibFuzzer.html)
- [AddressSanitizer 가이드](https://github.com/google/sanitizers/wiki/AddressSanitizer)

---

💡 **Tip**: 장시간 퍼징 시에는 `tmux`나 `screen`을 사용하여 세션을 유지하는 것을 권장합니다.

```bash
tmux new-session -d -s zlib_fuzzing
tmux send-keys -t zlib_fuzzing "./run.sh --all -t 7200" Enter
tmux attach -t zlib_fuzzing
```