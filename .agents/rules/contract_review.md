---
trigger: always_on
description: AirMouse 프로젝트의 모든 코드 수정, 기능 추가, 코드 리뷰 시 contract_doc의 5대 계약 산출물(CONTRACT, STATE, FLOW, BUDGET, SPEC)을 기준으로 정합성을 검증하는 불변 규칙입니다.
---

# Contract-Based Review & Development Rules

이 프로젝트의 소스 코드(`.h`, `.cpp`)를 수정, 신규 기능 추가 또는 리뷰할 때는 **반드시** 아래 경로에 위치한 5대 지속 산출물(Persistent Artifacts)을 대조 기준으로 삼아 정합성을 검증해야 합니다.

- 계약 문서 경로: `src/v0410/docs_v0410/contract_doc/`
  1. `CONTRACT.md`: 모듈 경계, 호출 권한, FreeRTOS 큐 계약, 공유 변수 동기화 및 원자성 계약
  2. `STATE.md`: 상태 변수 단독 소유권(Writer/Reader) 및 FSM(Click-Freeze, Snap, Precision, Macro) 명세
  3. `FLOW.md`: 모션 파이프라인, 프로파일 전환 시퀀스, 액션/제스처 디스패치 흐름
  4. `BUDGET.md`: FreeRTOS 태스크 주기(8ms/7ms/50ms), Worst-case 실행 시간, 블로킹 금지 규칙
  5. `SPEC.md`: 요구사항 ID ↔ 구현 소스 추적성 매트릭스

---

## 🚨 필수 검토 및 준수 6대 원칙 (Invariants)

1. **[CONTRACT] 4대 절대 금지 규칙 준수**:
   - **Web Task의 HID 직접 접근 금지**: `_mouse`, `_keyboard`는 오직 `commTask`만 제어하며, Web/Sensor는 `_qHidCmd` 또는 `_qActionExec`를 경유해야 합니다.
   - **Special 액션의 Context 격리**: `EN_C20_ACT_SPECIAL`은 큐에 넣지 않고 **오직 `sensorTask` 컨텍스트에서만 동기 실행**합니다.
   - **LED 상태머신의 단독 Tick**: `_led.tick()`은 오직 `_ledTask`(50ms 주기)에서만 단독 실행합니다.
   - **블로킹 금지**: 모든 큐 인큐(`_enqueueAction`, `_enqueueHidCmd`, `_pushFrame`)는 `timeout = 0` (non-blocking)을 유지해야 합니다.
2. **[STATE] 상태 소유권 침해 금지**:
   - `STATE.md` 소유권 매트릭스에 명시된 단독 Writer 외의 태스크에서 직접 쓰기 금지.
   - 다중 태스크가 접근하는 변수는 계약된 동기화 수단(`volatile`, `_lock()`)을 정확히 준수.
3. **[FLOW] 파이프라인 정합성 유지**:
   - 모션 파이프라인 필터 순서(Bias → M10 → FrontHold/MoveGate → Precision → Snap → Click-Freeze)를 임의로 변경하지 말 것.
   - 프로파일 변경 시 `_macroAbortToken` 증가 및 큐 드레인/리셋 위임 절차 필수 준수.
4. **[BUDGET] 시간 예산 및 블로킹 초과 금지**:
   - `sensorTask`(8ms), `commTask`(7ms) 내에서 루프를 지연시키는 블로킹 I/O, sleep, 지연 루프 금지.
   - 매크로 시퀀스는 블로킹 없이 `_tickMacro` 비동기 상태머신으로 처리.
5. **[SPEC] 추적성 유지**:
   - 새로운 기능 또는 동작 변경 시 `SPEC.md`의 요구사항 ID 및 구현 파일 링크를 갱신.
6. **코드 변경 후 문서 동기화**:
   - 인터페이스/큐/상태/예산/요구사항에 영향을 주는 코드 수정이 발생한 경우, 소스 파일과 함께 `contract_doc/`의 해당 문서도 함께 동기화하여 수정.

---

## 📋 일관된 리뷰 및 검토 출력 포맷

코드 작성 전 검토 또는 코드 리뷰 응답 시 아래 6개 표준 카테고리를 고정 적용합니다:

```markdown
### A. 계약 위반 검토 (CONTRACT.md 기준)
- 위반 여부, 위치, 심각도, 조치 사항

### B. 상태 소유권 위반 검토 (STATE.md 기준)
- Writer/Reader 위반, 동기화 누락 여부

### C. 시간 예산 및 블로킹 검토 (BUDGET.md 기준)
- 최악 실행 시간 영향, 태스크 데드라인 준수 여부

### D. 흐름 및 파이프라인 간섭 검토 (FLOW.md 기준)
- 기존 파이프라인 순서 및 분기 영향

### E. 요구사항 정합성 검토 (SPEC.md 기준)
- 요구사항 ID 충돌 여부 및 추적성

### F. 커버리지 및 최종 판정
- 종합 판정 (PASS / WARN / FAIL)
```
