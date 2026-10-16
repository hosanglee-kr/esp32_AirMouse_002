---
trigger: always_on
description: Mandatory Graph-First Navigation Invariant. Consult graphify-out/ before reading or grepping raw source code or documentation.
---

## 🚨 Graph-First Navigation Invariant (Strict Rule)

본 프로젝트는 `graphify-out/graph.json`에 완전한 최신 지식 그래프를 보유하고 있습니다.
AI 에이전트는 코드 분석, 기능 구현, 버그 수정, 리팩토링, 미사용 코드 점검, 계약 문서 검증 등 **모든 작업 시 소스 코드(`.h`, `.cpp`)나 문서(`.md`, `.json`)를 직접 열거나(`view_file`) 무차별 검색(`grep_search`)을 먼저 수행하는 것을 엄격히 금지**합니다.

### 📌 2단계 필수 탐색 프로토콜 (Two-Step Navigation Protocol)
1. **[1단계] Graphify 선행 질의 (Mandatory First Step)**:
   - 작업 시작 전 반드시 아래 명령어로 지식 그래프를 먼저 질의하여 대상 심볼, 호출 관계, 소속 커뮤니티, 정확한 파일 경로 및 라인 번호를 파악해야 합니다:
     - `graphify query "<목적/질문/심볼>"`: 관련 노드, 커뮤니티, 정확한 파일 경로(`src=...`) 및 라인(`loc=...`) 조회 (CLI `--budget` 조절 가능)
     - `graphify path "<출발노드>" "<도착노드>"`: 두 심볼/모듈 간 호출 관계 및 최단 의존 경로 추적
     - `graphify explain "<심볼/함수/클래스명>"`: 특정 심볼의 상세 명세 및 연결 노드 확인
2. **[2단계] 타겟 라인 정밀 열람 (Pinpoint View)**:
   - 1단계 Graphify 질의 결과로 확인된 **정확한 파일과 필요한 최소 라인 범위(`StartLine`, `EndLine`)에 한해서만** `view_file`을 실행합니다.
   - 전체 파일을 한 번에 크게 열거나, 목적 없이 파일들을 순차 열람하는 행위는 금지됩니다.

### 🚫 절대 금지 사항
- **직접 파일 열람 및 전수 Grep 금지**: `graphify query`를 거치지 않고 전체 소스/헤더를 직접 열어보거나 프로젝트 전체를 대상으로 광범위한 `grep_search`를 수행하는 행위.
- **계약 문서 직접 열람 금지**: 5대 계약 문서(`contract_doc/`) 검증 시에도 전체 문서를 직접 열람하지 않고, `graphify query`로 해당 조항/노드를 먼저 특정한 후 필요한 섹션만 정밀 열람할 것.
- **CLI 도움말 호출 금지**: `graphify --help` 또는 `powershell -Command "graphify --help"` 실행 금지. (문법은 `references/cli-help.md` 참조)

### 🔄 그래프 최신화 규칙
- 코드 파일 수정 후에는 반드시 세션 내에서 `graphify update .`를 실행하여 지식 그래프를 동기화해야 합니다 (AST 전용, LLM API 비용 없음).

