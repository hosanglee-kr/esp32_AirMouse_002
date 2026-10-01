# SPEC.md — 요구사항 추적성 매트릭스

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 원본 사양서: `src/v0410/docs_v0410/SPEC_Manual/00.SPEC_0410_001.md`  
> 최종 갱신: 2026-10-01  
> 위치: `src/v0410/docs_v0410/contract_doc/SPEC.md`

---

## 요구사항 ↔ 구현 매핑 매트릭스

| 요구사항 ID | 핵심 기능 및 요구사항 요약 | 사양서 위치 | 주요 구현 파일 및 함수/위치 | 상태 |
|---|---|---|---|:---:|
| **R-TASK-01** | Dual Core 태스크 분리 (Sensor Core 1, Comm Core 0) | SPEC §태스크 모델 | `E10_AirMouse_Core_0410.cpp:104` | ✅ |
| **R-HID-01** | Web/Sensor 직접 HID 제어 금지, 큐 경유 단일 실행자 | SPEC §데이터 흐름 | `E10_AirMouse_Task_0410.cpp:570` | ✅ |
| **R-SPEC-01** | Special 액션의 sensorTask 단독 동기 실행 | SPEC §Special 라우팅 | `E10_AirMouse_Action_0410.cpp:353` | ✅ |
| **R-MACRO-01**| 8×8 매크로 비동기 상태머신 (Zero Blocking commTask) | SPEC §매크로 실행 | `E10_AirMouse_Action_0410.cpp:398` | ✅ |
| **R-MACRO-02**| `_macroAbortToken` 기반 원자적 즉시 취소 | SPEC §매크로 실행 | `E10_AirMouse_Action_0410.cpp:256` | ✅ |
| **R-PROF-01** | 최대 5개 독립 프로파일 관리 및 인덱스 스위칭 | SPEC §프로젝트 개요 | `C10_Config_0410.cpp:24` | ✅ |
| **R-PROF-02** | 프로파일 전환 시 잔속 액션/매크로 해제 및 큐 드레인 | SPEC §프로파일 | `E10_AirMouse_Core_0410.cpp:621` | ✅ |
| **R-SLOT-01** | 27개 트리거 Global + Mode 1~3 Override 2단 리졸빙 | SPEC §아키텍처 | `E10_AirMouse_Core_0410.cpp:436` | ✅ |
| **R-LOCK-01** | 4대 필수 슬롯 잠금 (S1 좌클릭, S5 MoveGate, S12 모드, S13 페어링) | SPEC §필수 슬롯 잠금 | `E10_AirMouse_Action_0410.cpp:70` | ✅ |
| **R-FRZ-01**  | Click-Freeze 4단계 FSM (IDLE/LOCKED/HOLD/FADEOUT) | SPEC §물리 엔진 | `E10_AirMouse_Motion_0410.cpp:211` | ✅ |
| **R-SNAP-01** | Snap-to-Axis 소프트 축 감쇠 알고리즘 | SPEC §물리 엔진 | `E10_AirMouse_Motion_0410.cpp:141` | ✅ |
| **R-PREC-01** | Precision 3단계 FSM (ENTRY/TRACK/EXIT) 및 조이스틱 감쇠 | SPEC §물리 엔진 | `E10_AirMouse_Motion_0410.cpp:100` | ✅ |
| **R-GEST-01** | 제스처 3계층 (Flick, Linear, Mode 3 전용 Tilt Hold) | SPEC §제스처 시스템 | `M30_Gesture_0410.cpp` | ✅ |
| **R-SCRL-01** | Side F Front Hold 기반 수직/수평 휠 스크롤 | SPEC §Front Hold | `E10_AirMouse_Task_0410.cpp:360` | ✅ |
| **R-PWR-01**  | 60초 미조작 시 Light-sleep 및 MPU WoM/버튼 Wake | SPEC §전원 관리 | `P20_Power_0410.cpp:145` | ✅ |
| **R-PWR-02**  | 커서 이동 deferred activity 갱신 | SPEC §전원 관리 | `E10_AirMouse_Task_0410.cpp:60` | ✅ |
| **R-LED-01**  | `_ledTask` 단독 50ms tick 및 Mode별 색상 표시 | SPEC §LED | `E10_AirMouse_Task_0410.cpp:657` | ✅ |
| **R-BLE-01**  | 최대 3개 호스트 본딩 및 순환 연결 (Host Cycle) | SPEC §Multi-Host | `B20_Ble_0410.cpp:110` | ✅ |
| **R-FS-01**   | Schema v5 원자적 프로파일 IO (`.tmp` → `.old` → rename) | SPEC §Atomic Config | `C10_Config_0410.cpp:180` | ✅ |
| **R-SAFE-01** | SafeBoot 및 OTA Guard 게이트 | SPEC §아키텍처 | `E10_AirMouse_Task_0410.cpp:540` | ✅ |
