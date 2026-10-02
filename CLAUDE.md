# CLAUDE.md — Claude Code Project Guidelines

> Target Project: esp32_AirMouse_002 (v0410)
> Architecture: ESP32-S3-Zero + MPU6050 BLE HID Composite AirMouse

---

## 📌 Primary Reference: agent.md

Claude Code는 본 프로젝트의 기본 아키텍처, 하드웨어 핀맵, 네이밍 규칙, 빌드/업로드 명령 및 세부 작업 지침에 대해 **[agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)**를 최우선으로 참조하고 준수해야 합니다.

---

## 🚨 MANDATORY: Contract-Based Review & Development Protocol

모든 코드 수정, 신규 기능 추가 또는 코드 리뷰 시 Claude Code는 **반드시** 아래 경로에 위치한 5대 지속 산출물(Persistent Artifacts)을 대조 기준으로 삼아 정합성을 검증해야 합니다:

- 계약 문서 경로: [src/v0410/docs_v0410/contract_doc/](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0410/docs_v0410/contract_doc/)
  1. **CONTRACT.md**: 모듈 경계, 호출 권한, FreeRTOS 큐 계약, 공유 변수 동기화 및 원자성 계약
  2. **STATE.md**: 상태 변수 단독 소유권(Writer/Reader) 및 FSM(Click-Freeze, Snap, Precision, Macro) 명세
  3. **FLOW.md**: 모션 파이프라인(8ms), 프로파일 전환 시퀀스, 액션/제스처 디스패치 흐름
  4. **BUDGET.md**: FreeRTOS 태스크 주기(8ms/7ms/50ms), Worst-case 실행 시간, 블로킹 금지 규칙
  5. **SPEC.md**: 요구사항 ID <-> 구현 소스 추적성 매트릭스

---

## ⚡ 6 Invariant Development Rules

1. **[CONTRACT] 4대 절대 금지 규칙**:
   - **Web Task의 HID 직접 접근 금지**: _mouse, _keyboard는 commTask 단독 제어 (큐 경유 필수).
   - **Special 액션의 Context 격리**: EN_C20_ACT_SPECIAL은 sensorTask 컨텍스트에서만 동기 실행.
   - **LED 상태머신의 단독 Tick**: _led.tick()은 _ledTask(50ms)에서만 단독 실행.
   - **블로킹 금지**: 모든 큐 인큐(_enqueueAction, _enqueueHidCmd, _pushFrame)는 timeout = 0 유지.
2. **[STATE] 상태 소유권 침해 금지**: STATE.md의 단독 Writer 외 태스크 직접 쓰기 금지.
3. **[FLOW] 파이프라인 정합성 유지**: 모션 파이프라인 순서 보존 및 프로파일 전환 시 큐 드레인/리셋 위임 절차 준수.
4. **[BUDGET] 시간 예산 및 블로킹 초과 금지**: sensorTask(8ms), commTask(7ms) 내 블로킹 금지, 매크로는 _tickMacro 비동기 FSM 처리.
5. **[SPEC] 추적성 유지**: 신규 기능/변경 시 SPEC.md 매핑 갱신.
6. **코드 변경 후 문서 동기화**: 인터페이스/상태/예산 영향 시 contract_doc 문서 함께 동기화.

---

## 📋 Standard Review Response Format

코드 리뷰 또는 작성 전 검토 시 아래 6개 표준 카테고리를 고정 적용합니다:

`markdown
### A. 계약 위반 검토 (CONTRACT.md 기준)
### B. 상태 소유권 위반 검토 (STATE.md 기준)
### C. 시간 예산 및 블로킹 검토 (BUDGET.md 기준)
### D. 흐름 및 파이프라인 간섭 검토 (FLOW.md 기준)
### E. 요구사항 정합성 검토 (SPEC.md 기준)
### F. 커버리지 및 최종 판정
`
