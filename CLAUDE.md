# CLAUDE.md — Claude Code Project Guidelines

> **Target Project**: `esp32_AirMouse_002` (Elite AirMouse S3)  
> **Firmware Version**: `v0415` (Active)  
> **Architecture**: ESP32-S3-Zero + MPU6050 BLE HID Composite AirMouse  
> **Single Source of Truth (SSOT)**: [agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)

---

## 📌 Single Source of Truth: agent.md 최우선 참조 안내

본 프로젝트의 모든 규정, 아키텍처 원칙, 하드웨어 핀맵, 코딩 및 명명 규칙, FreeRTOS 멀티태스킹 동기화 계약, 5대 계약 문서(CONTRACT, STATE, FLOW, BUDGET, SPEC) 기반 개발/리뷰 프로토콜, 필수 개발 6대 원칙, 표준 리뷰 응답 포맷(A~F), 빌드 및 검증 명령어 등은 **[agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)**에서 단일 통합 관리됩니다.

Claude Code는 분석, 설계, 구현, 리팩토링, 코드 리뷰, 검증 등 본 프로젝트의 모든 작업을 수행할 때 **반드시 [agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)의 최신 지침을 최우선으로 참조하고 엄격히 준수**해야 합니다.

모든 신규 규칙 추가, 정책 수정 및 현행화 작업은 `agent.md`를 단일 기준으로 수정 및 반영합니다.

---

## 🚨 핵심 준수 4대 헌장 (Core Invariants Quick Reference)

Claude Code는 작업을 시작하기 전 아래 4대 핵심 헌장을 절대 위반하지 않아야 합니다:

1. **[Graph-First Navigation] 소스/문서 직접 열람 및 무차별 Grep 금지**:
   - 코드를 분석하거나 수정할 때 소스나 문서를 직접 크게 열거나 전수 검색하지 말고, **반드시 `graphify query / path / explain`을 먼저 실행**하여 대상 파일과 라인 번호를 특정한 뒤 정밀 열람하십시오.
2. **[CONTRACT Invariants] 4대 절대 금지 규칙 준수**:
   - **Web Task의 HID 직접 접근 금지**: `_mouse`, `_keyboard`는 오직 `commTask`만 제어 (큐 경유 필수).
   - **Special 액션의 Context 격리**: `EN_C20_ACT_SPECIAL`은 오직 `sensorTask` 컨텍스트에서만 동기 실행.
   - **LED 상태머신의 단독 Tick**: `_led.tick()`은 오직 `_ledTask`(50ms)에서만 단독 실행.
   - **블로킹 금지**: 모든 큐 인큐(`_enqueueAction`, `_enqueueHidCmd`, `_pushFrame`)는 `timeout = 0` (non-blocking) 유지.
3. **[Standard Review Format] 코드 리뷰 시 6대 표준 포맷(A~F) 고정 적용**:
   - 계약 위반(A), 상태 소유권(B), 시간 예산/블로킹(C), 흐름/파이프라인(D), 요구사항 정합성(E), 종합 판정(F) 카테고리 준수.
4. **[Pre-Commit Integrity Check] 수정 후 무결성 검증 필수**:
   - 코드나 설정 변경 후 `python src/v0415/tools_v0415/check_i18n_0415.py` 및 `python src/v0415/tools_v0415/check_schema_0415.py` 실행하여 정합성을 확인하고, `graphify update .`로 지식 그래프를 갱신합니다.

👉 **상세 아키텍처, 핀맵, 네이밍 규칙, 검증 명령어 및 전체 프로토콜은 [agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)를 확인하십시오.**
