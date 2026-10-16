# Graph Report - esp32_AirMouse_002  (2026-10-16)

## Corpus Check
- 119 files · ~311,179 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 25 file(s) not represented in the graph (top: .ino 12, (none) 8, .csv 1)

## Summary
- 3028 nodes · 4407 edges · 246 communities (210 shown, 36 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 701 edges (avg confidence: 0.88)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `015358e7`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Missing UI Features / Gaps:
- CL_E10_EliteAirMouse::_sensorTask
- CL_C20_BtnDispatcher
- E10_AirMouse_0415.h
- setup
- check_schema_0415.py
- ST_C10_MacroLib_t
- NimBLEAddress
- check_i18n_0415.py
- qs
- 6\. 반영 우선순위 (실행 순서)
- ST_E10_DiagCounters
- 🎬 Action Registry & 매크로 라이브러리
- [v0415] Multi-Host 전환 기능 핵심 구현 방안 및 기본 설계서
- 🏗️ 아키텍처
- JsonObjectConst
- portMUX_TYPE
- C10_Def_0415.h
- ST_C10_WiFiConfig_t
- 2.10 버튼 이벤트 시퀀스 계약 (v0415, C20 patch)
- 📡 Multi-Host 시스템 (Option A: Reject-Until-Target)
- 2. 결함 검토 및 분석 결과 (표준 6 카테고리)
- 🔴 Critical — 즉시 수정 필요 (6건)
- 7. 단계별 실행 계획 (Phase 0 ~ Phase 5 Roadmap)
- 4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules)
- 6. 계약 및 5대 아티팩트 정합성 검토 (Contract Compliance Matrix)
- 3. ⚡ 개발 환경 및 필수 명령어
- 💡 LED (L10)
- 🔋 전원 관리 (P20)
- _ensure_verify_done
- a40_comfunc_070
- 2\. 추가 발견 이슈 (5건)
- 🎯 최종 권장 실행안
- 🎮 제스처 시스템 (M30)
- 🖥️ 프론트엔드 (Web UI)
- 4. 구체적 구현 설계 명세 (Detailed Implementation Spec)
- 1. v0412 아키텍처 개요
- _execMouseWheel
- 🟢 Minor — 문서·정합성 (4건)
- 2. 해결 방안 4가지 비교
- 4. 단계별 도입 계획
- 5. 모듈별 상세 구현 명세 (Detailed Module Specifications)
- 1. 개요 및 배경 (Executive Summary & Background)
- 8. Web REST API 명세 (확장)
- 5. 🛠️ 빌드 시스템 (PlatformIO CLI)
- 📐 명명 규칙 & 코드 정책
- 📎 부록: 주요 상수
- 🔌 하드웨어
- 14. Phase P-A 구현 명세 및 헤더 초안
- 16-1. P-J-a — 매크로 실행 중 Status 피드백 (`macro_running`)
- 16-6. v0412+ — Profile × Multi-Host Peer 자동 연동
- 1\. 현재 프로젝트 상태 vs Option A 요구사항
- 4\. Critical 2차 수정 상세 코드
- 2. 핵심 메커니즘 및 아키텍처 원칙 (Core Architecture)
- _clamp
- ST_E10_PrecProfile_t
- _kbUsageLabel
- 4. 데이터 스키마 및 영속성 설계 (Schema & Persistence)
- 🚀 빌드
- CL_E10_EliteAirMouse::begin
- 10. 충전 및 절전 모드
- asynctcp
- c10_config_0310
- c10_def_0303
- d10_logger_061
- e10_airmouse_0309
- e10_def_0302
- e10_def_0310
- keyboardconfiguration
- keyboardhidcodes
- m10_motionproc_0300
- type_traits
- utility
- w10_const_0277
- w10_def_0301
- w10_webconfig_0304
- w10_webconfig_0310
- 5\. 프로젝트 적용 권장 순서
- D10_Logger_0415.h
- c10_config_0304
- e10_airmouse_0310
- d10_logger_0410
- main.cpp
- LD2420
- 📝 AirMouse Elite S3 (v0410) 종합 가이드 & 기술 사양서
- 버튼·기능 설계 — 요구사항 및 용어 표준화 사양서
- 0415_변경_001.md
- [v0415] Multi-Host 전환 기능(Option A: Reject-Until-Target) 통합 상세 설계서
- C10_Config_Valid_0415.cpp
- CL_C10_Config::patchProfileFromJson
- CL_E10_EliteAirMouse
- 🤖 Agent Guide & Project Instructions (agent.md)
- 0415_변경_002.md
- 📊 파워포인트 프레젠테이션 모드 단축키 요약
- CL_W10_WebConfig
- a40_comfunc_0410
- c10_config_0410
- e10_airmouse_0410
- w10_web_0410
- 🚨 Graph-First Navigation Invariant (Strict Rule)
- workflows/graphify.md
- ci_002.yml.md
- pio_gzip_0415.py
- ._pushFrame
- LD2420.cpp
- CL_C10_Config
- 16. 자주 묻는 문제 및 해결책
- CL_L10_Led
- ST_C10_HostsConfig_t
- 17. 자주 묻는 문제 (FAQ)
- ST_E10_Status_t
- CL_A40_MutexGuard_Semaphore
- W10_Def_0415.h
- 향후 개선 (선택, v0416 후보)
- LD2420_Proto_001.cpp
- 7.10. 포인팅 정밀도 & 모션 알고리즘 고도화 (v0411)
- 2\. 적용 단계별 계획 (4 Stage)
- Backend features to check for frontend gaps:
- library.json
- Elite AirMouse S3 — v0415 기술 사양서 (정본)
- Elite AirMouse S3 — 사용자 매뉴얼
- CL_C20_ActionExec
- CL_M20_BiasTracker
- 🔍 예제별 상세 설명
- LD2420Calibration
- .isSelecting
- API 레퍼런스
- LD2420 Arduino Library
- LD2420.h
- LD2420TargetData
- [0.0.1] - 2026-10-06
- CL_M10_AdvancedMotionProcessor
- CL_M30_Gesture
- ST_C10_E10Config_t
- am_base_0415.js
- W10_Web_Static_0415.cpp
- AsyncWebServerRequest
- CL_D10_Logger
- A40_ComFunc_0415.h
- FLOW_0415.md — 기능별 데이터 흐름 및 파이프라인 명세
- E10_AirMouse_Action_0415.cpp
- 📝 AirMouse Elite S3 (v0415) 종합 가이드 & 기술 사양서
- E10_AirMouse_Core_0415.cpp
- CL_P20_Power
- W10_WebApi_Com_0415.cpp
- buildWakeMaskSafe
- CL_B20_Ble
- ST_C20_ActionSlot_t
- 14. 포인터 및 센서 세부 설정 (Config 탭)
- am_status_0415.js
- Elite AirMouse S3 — v0412 백엔드 논리/크리티컬 결함 점검 및 개선 요구서
- C10_Config_Json_0415.cpp
- W10_WebApi_Status_0415.cpp
- 2. 상태머신(FSM) 상세 명세
- 16. v0412 잔여 유보 이슈 상세 및 대응 방안 (Deferred Issues)
- am_profile_0415.js
- am_remote_0415.js
- 6. v0415 리팩터 반영
- 백엔드 ↔ 프론트엔드 상수 공유 방안 검토
- 웹 기반 사용자 개별화 고도화 — 통합 구현 계획서 (v0412)
- 2. 함수 인터페이스 계약
- ST_C10_Macro_t
- Part 3: v0412 핵심 기술 사양 요약 (Summary Specification)
- E10_AirMouse_Diag_0415.cpp
- 🎛️ Mode 시스템
- ST_C10_ProfileConfig_t
- 10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11)
- docs_v0415/Req_ImplPlan/30.impl_0410_002.md
- E10_AirMouse_Hid_0415.cpp
- 2. 도메인별 표준 용어 매핑 테이블
- 7. 🚀 v0415 핵심 기능 상세 사양
- W10_Web_init_0415.cpp
- ST_C10_BootState_t
- ST_C10_MotionAdv_Ema_t
- ST_C10_PowerConfig_t
- 15. 진단 및 입력 테스트 화면
- 사용자 개별화 기능 고도화 — 검토
- 웹 페이지 기반 사용자 개별화 — 고도화 검토
- 웹 기반 사용자 개별화 고도화 — 통합 구현 계획서 (v0412)
- ST_C10_ButtonConfig_t
- 🟢 A. 프로파일 세트 (권장)
- 7.10. 포인팅 정밀도 & 모션 알고리즘 고도화 (v0412)
- ST_C10_MotionAdv_ClickFreeze_t
- app_0415_0001.js
- am_macro_0415.js
- handleOfflineApi
- Elite AirMouse S3 — 사용자 매뉴얼
- 13. 버튼 및 동작 맞춤 할당
- W10_Web_0415.h
- ST_C10_MotionAdv_Snap_t
- 🛡️ SafeBoot / OTA Guard
- 📝 Config & Profile System (Schema 411)
- CONTRACT_0415.md — 모듈 경계 및 인터페이스 계약
- am_host_0415.js
- am_i18n_0415.js
- 6. A 도입 시 설계 요약
- Elite AirMouse S3 기능적 관점 보완 사항
- 12. 버튼 및 동작 맞춤 할당 화면 (슬롯 관리)
- 15. 세부 설정 (포인터 및 센서)
- 16. 진단 및 문제 해결
- 7. 모드 3: 스마트 TV 모드 (표시등: 주황색)
- E10_AirMouse_Task_0415.cpp
- ST_MacroState_t
- 11. 브라우저 기반 상세 설정 화면 (웹 UI)
- 13. 자동 연속 동작 화면 (매크로 관리)
- 5. 모드 1: PC 일반 작업 모드 (표시등: 파란색)
- 6. 모드 2: 발표 모드 (표시등: 초록색)
- 9. 기기 재연결 및 다중 장치(멀티 호스트) 관리
- 10. 충전과 자동 절전
- 2. 아키텍처 결정 (4대 핵심 축)
- docs_v0415/Req_ImplPlan/30.impl_0410_001.md
- 14. 매크로 관리 (자동 연속 동작)
- 3. 구성품 및 처음 사용하기
- 5. 모드 1: PC 모드 (파란 불빛)
- 6. 모드 2: 발표 모드 (초록 불빛)
- 7. 모드 3: 스마트 TV 모드 (주황 불빛)
- 9. 여러 대의 컴퓨터/TV 관리하기
- 3. 웹 UI 구성 옵션 (3단계)
- 4. 트리거 자유도의 세부 옵션
- 7. 실시간 피드백 (UX 필수)
- 15. 검증 체크리스트 (P-J)
- 3. Config 스키마 (v5 통합 사양)
- 5. Global + Override 메커니즘
- 5. 웹 REST API 사양 (v0412)
- 6. 실행 흐름 (Runtime Flow)
- 7. Web UI 3-View 통합 설계
- 3. 기본 연결 및 작동법
- 11. 웹 화면으로 상세 설정하기
- ST_E10_ErrEvt_t
- ST_C10_ProfileSlots_t
- docs_v0415/Req_ImplPlan/30.impl_0412_002.md
- docs_v0415/Req_ImplPlan/40.codereview_T1_001.md
- 12. 홈 탭 사용하기
- ST_Runtime_t
- 20. 사용자 지원
- 8. 모드 전환과 상태 표시등
- am_offline_data_0415.js
- 6. 📐 명명 규칙 & 코드 정책
- am_config_motion_0415.js
- 7.11. 전원 관리 시스템 (P20 & Light/Deep Sleep & WoM)

## God Nodes (most connected - your core abstractions)
1. `CL_E10_EliteAirMouse` - 213 edges
2. `CL_W10_WebConfig` - 106 edges
3. `LD2420` - 86 edges
4. `CL_C10_Config` - 79 edges
5. `ST_E10_Status_t` - 54 edges
6. `AsyncWebServerRequest` - 50 edges
7. `ST_C10_E10Config_t` - 48 edges
8. `CL_L10_Led` - 45 edges
9. `CL_B20_Ble` - 42 edges
10. `ST_C20_ActionSlot_t` - 42 edges

## Surprising Connections (you probably didn't know these)
- `7.11.1. 설정 파라미터 (`ST_C10_PowerConfig_t`)` --references--> `ST_C10_PowerConfig_t`  [INFERRED]
  README_0415_001.md → src/v0415/C10_Def_0415.h
- `4.2. 프론트엔드 8-모듈 구조 (`data_v0415_www/`)` --references--> `W10_hasVersionToken()`  [INFERRED]
  README_0415_001.md → src/v0415/W10_Def_0415.h
- `4.2. 프론트엔드 7-모듈 구조` --references--> `W10_hasVersionToken()`  [INFERRED]
  README.md → src/v0415/W10_Def_0415.h
- `① 명명 규칙 (Strict Naming Conventions)` --references--> `CL_C10_Config`  [INFERRED]
  agent.md → src/v0415/C10_Config_0415.h
- `10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11)` --references--> `ST_C10_MotionAdv_t`  [INFERRED]
  README_0415_001.md → src/v0415/C10_Def_0415.h

## Import Cycles
- None detected.

## Communities (246 total, 36 thin omitted)

### Community 0 - "Missing UI Features / Gaps:"
Cohesion: 0.16
Nodes (18): bindSlotsAndConfig(), unwrapApi(), cfgExport(), ctlForceRelease(), safeBootExit(), loadTriggers(), diagClear(), otaStatus() (+10 more)

### Community 1 - "CL_E10_EliteAirMouse::_sensorTask"
Cohesion: 0.14
Nodes (15): _applyClickFreeze, _applyPrecision, _applySnapToAxis, _bootPriorityReconnect, _consumeHostOp, _consumeResetAndAsyncFlags, _fsmUpdate, _handleGesture (+7 more)

### Community 2 - "CL_C20_BtnDispatcher"
Cohesion: 0.07
Nodes (36): EN_Phase_t, EventCallback, CL_C20_BtnDispatcher, begin, _btn, _cb, _checkTimers, _ctx (+28 more)

### Community 3 - "E10_AirMouse_0415.h"
Cohesion: 0.18
Nodes (12): Adafruit_MPU6050, Adafruit_NeoPixel, adafruit_sensor, arduino, BleCompositeHID, math, ble_gap_event, KeyboardDevice (+4 more)

### Community 4 - "setup"
Cohesion: 0.12
Nodes (16): _holdAtBoot(), setup(), setLevel, 1. 태스크 주기 및 시간 예산, 2.1 sensorTask 컨텍스트, 2.2 commTask 컨텍스트, 2.3 매크로 step 실행 정책 (v0415 명확화), 2.4 D10 로깅 및 비동기 리부트 블로킹 제한 규칙 (rev11) (+8 more)

### Community 5 - "check_schema_0415.py"
Cohesion: 0.19
Nodes (18): check_b20_host_constants(), check_bundle_completeness(), check_e10_json_parity(), check_e10_nested_mapping(), check_gesture_removed(), check_w10_err_map_sorted(), check_w10_kb_names_sorted(), check_wave_cd_dom_ids() (+10 more)

### Community 6 - "ST_C10_MacroLib_t"
Cohesion: 0.17
Nodes (12): JsonArray, _buildMacroJson, _buildSlotJson, makeDefaultsMacros, validateMacro, CL_C10_Config::_buildMacrosJson(), CL_C10_Config::_buildSlotArrayJson(), CL_C10_Config::validateMacros() (+4 more)

### Community 7 - "NimBLEAddress"
Cohesion: 0.20
Nodes (11): NimBLEAddress, getSpecialFriendlyName(), §14 다음 단계, 3-4. 파급 파일 일괄 갱신 (5개), 6\. 최종 판정, M-1. Special Action 값 확장의 파급 효과 (§8), M-2. Special 슬롯과 기존 Host Cycle 공존 정책 부재 (§8), M-3. 부팅 시 last\_host\_addr 자동 select의 부작용 (§6.1 P0-4) (+3 more)

### Community 8 - "check_i18n_0415.py"
Cohesion: 0.25
Nodes (10): pathlib, extract_keys_from_block(), find_block_start(), _find_duplicates(), main(), check_i18n_0415.py — i18n KO/EN 키 정합성 검증 - am_i18n_0415.js에서 I18N_DICT.ko /…, start_idx 위치의 '{' 부터 매칭되는 '}' 까지 라인을 순회하며 KEY_PATTERN에 매칭되는 키를 수집., 문자열 리터럴 내부의 중괄호를 무력화 (depth 계산용). (+2 more)

### Community 9 - "qs"
Cohesion: 0.24
Nodes (11): apiGet(), apiPostJson(), hideLoading(), hideProgress(), qs(), setAppMode(), setMsg(), setProgress() (+3 more)

### Community 10 - "6\. 반영 우선순위 (실행 순서)"
Cohesion: 0.29
Nodes (7): factoryReset(), 6\. 반영 우선순위 (실행 순서), 🔴 Phase 0 — 선행 검증 (0.5일), 🔴 Phase 1 — 기반 (0.5일), 🟡 Phase 3 — Special 8종 (0.5일), 🟡 Phase 4 — W10 + 프론트 (1.5일), 🟢 Phase 5 — Factory Reset + NVS (0.5일)

### Community 11 - "ST_E10_DiagCounters"
Cohesion: 0.18
Nodes (10): 14. 개정 이력, ST_E10_DiagCounters, consecutiveFail, consecutiveI2cFail, consecutiveRecoverFail, errMpuNan, errMutexMiss, errTaskOverrun (+2 more)

### Community 12 - "🎬 Action Registry & 매크로 라이브러리"
Cohesion: 0.22
Nodes (9): getConsumerFriendlyName(), Action Kind, 🎬 Action Registry & 매크로 라이브러리, Consumer Mask (`EN_C20_Consumer_t`, v0415 Descriptor 정합), KB Modifier, Slot 구조, Special Action, 매크로 시퀀서 규칙 (8×8) (+1 more)

### Community 13 - "[v0415] Multi-Host 전환 기능 핵심 구현 방안 및 기본 설계서"
Cohesion: 0.22
Nodes (8): 1. 개요 및 설계 방향, 2. 제외 및 단순화 항목 (고위험·불필요 복잡도 제거), 3. 핵심 확정 기능 범위 (Minimal Viable Scope), 5. 🤔 핵심 의사결정 필요 항목 (작동 예시 및 선택), [v0415] Multi-Host 전환 기능 핵심 구현 방안 및 기본 설계서, [의사결정 1] 목표 호스트 연결 실패 시 폴백(Fallback) 동작 방식, [의사결정 2] 물리 버튼을 통한 호스트 전환 조작 방식, [의사결정 3] 에어마우스 전원을 켰을 때(부팅 시) 첫 연결 우선순위

### Community 14 - "🏗️ 아키텍처"
Cohesion: 0.25
Nodes (8): main.cpp 부팅 시퀀스, Mutex 정책, 데이터 흐름 (반드시 준수), 매크로 실행 (비동기 상태머신), 상태 소유권, 🏗️ 아키텍처, 태스크 간 Special 액션 라우팅, 태스크 모델

### Community 15 - "JsonObjectConst"
Cohesion: 0.22
Nodes (9): JsonArrayConst, JsonObjectConst, clampVal(), T, Json_copyStrReq(), Json_getArr(), Json_getBool(), Json_getNum() (+1 more)

### Community 16 - "portMUX_TYPE"
Cohesion: 0.33
Nodes (6): portMUX_TYPE, CL_A40_muxGuard_Critical, _flagSpinlock, Dirty_clearAtomic(), Dirty_readAtomic(), Dirty_setAtomic()

### Community 17 - "C10_Def_0415.h"
Cohesion: 0.33
Nodes (6): makeProfilePath(), makeProfileTmpPath(), ST_C10_ProfileIndex_t, activeIndex, profileCount, 6.10 C10 원자적 저장 rotate 및 idx 가드 (v0415 rev9 신규)

### Community 18 - "ST_C10_WiFiConfig_t"
Cohesion: 0.25
Nodes (8): ST_C10_WiFiConfig_t, ap_pass, ap_ssid, mdns_host, mode, sta_pass, sta_ssid, 1\. WiFi 설정 전체 누락 (🔴 Critical)

### Community 19 - "2.10 버튼 이벤트 시퀀스 계약 (v0415, C20 patch)"
Cohesion: 0.29
Nodes (7): 1) DOWN/UP 안정 상태 전이, 2.10 버튼 이벤트 시퀀스 계약 (v0415, C20 patch), 2) CLICK (release 후 재입력 없음), 3) DOUBLE (double 윈도우 내 두 번째 stable DOWN), 4) LONG/HOLD_2S/HOLD_3S (progressive 누적 발화), 5) resetButton 타임스탬프 초기화 (R-1 fix), 6) update() 호출 전제조건

### Community 20 - "📡 Multi-Host 시스템 (Option A: Reject-Until-Target)"
Cohesion: 0.29
Nodes (7): 1. 개요 및 설계 결정, 2. 요구사항 추적성 매트릭스 (Requirements Traceability Matrix), 3. FSM 및 상태 전이 규칙, 4. W10 Web API 엔드포인트 명세, 5. C10 영구 스토리지 명세 (`hosts_0415.json`), 6. Special Action 상세 명세 및 버튼 매핑, 📡 Multi-Host 시스템 (Option A: Reject-Until-Target)

### Community 21 - "2. 결함 검토 및 분석 결과 (표준 6 카테고리)"
Cohesion: 0.29
Nodes (7): 2. 결함 검토 및 분석 결과 (표준 6 카테고리), A. 계약 위반 검토 (CONTRACT.md 기준), B. 상태 소유권 위반 검토 (STATE.md 기준), C. 시간 예산 및 블로킹 검토 (BUDGET.md 기준), D. 흐름 및 파이프라인 간섭 검토 (FLOW.md 기준), E. 요구사항 정합성 검토 (SPEC.md 기준), F. 종합 판정

### Community 22 - "🔴 Critical — 즉시 수정 필요 (6건)"
Cohesion: 0.29
Nodes (7): C-1. NimBLE 2.5.1 콜백 시그니처 오류 (§2.2), C-2. NimBLEDevice::getBondedDevices() 미존재 (§2.2), C-3. setOwnAddrType() 인자 개수 (§5.3), C-4. ble\_hs\_pvcy\_rpa\_config(1) 직접 호출 위치 (§5.3), C-5. \_qHostCmd 신규 큐 — 기존 CONTRACT §3 위반 (§5.1), C-6. host\_select\_policy 스키마 위치 (§7.1, §7.3), 🔴 Critical — 즉시 수정 필요 (6건)

### Community 23 - "7. 단계별 실행 계획 (Phase 0 ~ Phase 5 Roadmap)"
Cohesion: 0.29
Nodes (7): 7. 단계별 실행 계획 (Phase 0 ~ Phase 5 Roadmap), 🟢 Phase 0 — 라이브러리(Lib) 선행 검증 완료 보고 (V-1 ~ V-4 확정), 🔴 Phase 1 — 기반 인프라 구축 (0.5일), 🔴 Phase 2 — B20 Reject-Until-Target 엔진 구현 (2.0일), 🟡 Phase 3 — Special Action 8종 및 위임 연동 (0.5일), 🟡 Phase 4 — W10 Web API 7종 및 웹 UI (1.5일), 🟢 Phase 5 — NVS FIFO 동기화 및 안정화 (0.5일)

### Community 24 - "4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules)"
Cohesion: 0.33
Nodes (6): ① 명명 규칙 (Strict Naming Conventions), ② ArduinoJson v7 단일화 정책, ③ 문자열 및 버퍼 관리 정책, 4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules), ④ 원자적 스토리지 저장 정책 (Atomic Storage 5-Phase), ⑤ Mermaid 다이어그램 렌더링 무결성 규칙 (GitHub & Obsidian 완벽 호환)

### Community 25 - "6. 계약 및 5대 아티팩트 정합성 검토 (Contract Compliance Matrix)"
Cohesion: 0.29
Nodes (7): 6. 계약 및 5대 아티팩트 정합성 검토 (Contract Compliance Matrix), A. 계약 위반 검토 (`CONTRACT_0415.md` 기준), B. 상태 소유권 위반 검토 (`STATE_0415.md` 기준), C. 시간 예산 및 블로킹 검토 (`BUDGET_0415.md` 기준), D. 흐름 및 파이프라인 간섭 검토 (`FLOW_0415.md` 기준), E. 요구사항 정합성 검토 (`SPEC_0415.md` 기준), F. 커버리지 및 최종 판정

### Community 26 - "3. ⚡ 개발 환경 및 필수 명령어"
Cohesion: 0.33
Nodes (6): 📌 2단계 필수 탐색 프로토콜 (Two-Step Navigation Protocol), 3. ⚡ 개발 환경 및 필수 명령어, 🛠️ PlatformIO CLI 명령어, 🌐 정합성 무결성 검증 스크립트 (Pre-Commit Gate), 🧠 지식 그래프 (Graphify) 기반 탐색 원칙 (Strict Invariant), 🔄 지식 그래프 갱신

### Community 27 - "💡 LED (L10)"
Cohesion: 0.33
Nodes (6): 💡 LED (L10), Thread-safety (v0415), 상수 (v0415), 상태머신, 색상, 피드백

### Community 28 - "🔋 전원 관리 (P20)"
Cohesion: 0.33
Nodes (6): Light-sleep 조건 (모두 만족), Light-sleep 진입/복귀 (v0415 Safe/Pairing 분기), SLEEP_NOW 즉시 진입 (v0415), Wake 소스 (EXT1, LOW Active), 설정 구조체 (`ST_C10_PowerConfig_t`), 🔋 전원 관리 (P20)

### Community 29 - "_ensure_verify_done"
Cohesion: 0.25
Nodes (8): _ensure_verify_done(), 단일 검증 스크립트 실행. 성공(exit 0) 시 True, 실패 시 False., 전체 검증 시퀀스. 실패 시 SystemExit. G_SKIP_VERIFY=1로 우회 가능., 검증이 프로세스 내에서 1회만 실행되도록 보장., [개선 2] buildprog pre-action: 검증만 실행 (gzip 동기화 없음). 목적: 일반 빌드(`pio run -e…, run_verification(), run_verify_only(), _run_verify_script()

### Community 31 - "2\. 추가 발견 이슈 (5건)"
Cohesion: 0.33
Nodes (6): 2\. 추가 발견 이슈 (5건), 🔴 C-5. reject reason 판정의 파일 I/O 콜백 위험, 🔴 C-6. reject reason 코드 시맨틱 실측 필수, 🟡 C-7. \_whitelistActive와 \_rejectMode 의미 충돌, 🟡 C-8. reject 진입 시 현재 연결 peer 처리 누락, 🟡 C-9. sleep 방지 정책의 실제 구현 미제시

### Community 32 - "🎯 최종 권장 실행안"
Cohesion: 0.40
Nodes (5): Phase A (즉시, 1~2일), Phase B (1주), Phase C (1주, 선택), v0416 이월 항목, 🎯 최종 권장 실행안

### Community 33 - "🎮 제스처 시스템 (M30)"
Cohesion: 0.40
Nodes (5): Flick (Peak-to-Peak), Linear (임펄스 적분), Tilt Hold, 🎮 제스처 시스템 (M30), 활성 조건 (Mode별)

### Community 34 - "🖥️ 프론트엔드 (Web UI)"
Cohesion: 0.40
Nodes (5): 웹 진입 및 라우팅 규약 (Direct Serving), 자동 빌드 파이프라인 (`pio_gzip_0415.py`), 캐시 및 압축 정책, 파일 구조 (소스 분리 ↔ 런타임 단일 번들), 🖥️ 프론트엔드 (Web UI)

### Community 35 - "4. 구체적 구현 설계 명세 (Detailed Implementation Spec)"
Cohesion: 0.40
Nodes (4): 4.1 REQ-FIX-01: 레거시 중복 Power 블록 완전 제거, 4.2 REQ-FIX-02: `execLiveTest` Special 액션 비동기 위임, 4.3 REQ-FIX-03: `switchProfile` HID Release 보장 (Stuck Key 방지), 4. 구체적 구현 설계 명세 (Detailed Implementation Spec)

### Community 36 - "1. v0412 아키텍처 개요"
Cohesion: 0.40
Nodes (5): 1-1. 핵심 사양 변화 매트릭스, 1-2. 데이터 계층 다이어그램, 1-3. 태스크 모델 및 데이터 흐름, 1-4. Global + Override 슬롯 해석 ($O(1)$), 1. v0412 아키텍처 개요

### Community 37 - "_execMouseWheel"
Cohesion: 0.60
Nodes (5): CL_C10_Config::validateMacroStep(), CL_C10_Config::validateSlot(), C20_WheelAxis(), C20_WheelDir(), _execMouseWheel

### Community 38 - "🟢 Minor — 문서·정합성 (4건)"
Cohesion: 0.40
Nodes (5): m-1. 문서 통합 위치 (§12), m-2. BUDGET 갱신 누락 (§12), m-3. i18n 키 개수 추정 부재 (§6, §11), m-4. 프론트엔드 모듈 확장 위치 (§6.3 P2-1), 🟢 Minor — 문서·정합성 (4건)

### Community 39 - "2. 해결 방안 4가지 비교"
Cohesion: 0.40
Nodes (5): 2. 해결 방안 4가지 비교, 방안 A: 런타임 API (현행 확장), 방안 B: 빌드타임 코드 생성 (X-macro 파싱) ⭐ 권장, 방안 C: 외부 JSON 스키마 (순수 SSOT), 방안 D: 헤더 파일을 JS로 직접 파싱 (프론트가 C++ 파싱)

### Community 40 - "4. 단계별 도입 계획"
Cohesion: 0.40
Nodes (5): 4. 단계별 도입 계획, Phase 1 (0.5일) — 파일럿: Consumer 계열만, Phase 2 (0.5일) — Special + Kind + Mode 확장, Phase 3 (0.5일) — Trigger + Slot + KB, Phase 4 (0.5일) — 버전 검증 + 문서

### Community 41 - "5. 모듈별 상세 구현 명세 (Detailed Module Specifications)"
Cohesion: 0.40
Nodes (5): 5.1 B20 BLE Manager (`B20_Ble_0415.h`, `.cpp`), 5.2 C20 Action & E10 Task 연동 (신규 큐 대체 및 동기 위임), 5.3 W10 Web API (`W10_WebApi_Hosts_0415.cpp`), 5.4 프론트엔드 모듈 아키텍처 9-모듈 확장, 5. 모듈별 상세 구현 명세 (Detailed Module Specifications)

### Community 42 - "1. 개요 및 배경 (Executive Summary & Background)"
Cohesion: 0.40
Nodes (5): 1.1 문제 정의 및 요구사항, 1.2 선행 작업 요약 (NimBLE 2.5.1 API 정합 Hotfix), 1.3 메커니즘 채택 비교, 1.4 핵심 목표 및 성공 지표, 1. 개요 및 배경 (Executive Summary & Background)

### Community 43 - "8. Web REST API 명세 (확장)"
Cohesion: 0.50
Nodes (4): 8-1. 프로파일 관리 API, 8-2. 매크로 및 액션 API, 8-3. 슬롯 편집 및 레거시 API 정책, 8. Web REST API 명세 (확장)

### Community 44 - "5. 🛠️ 빌드 시스템 (PlatformIO CLI)"
Cohesion: 0.67
Nodes (3): 5. 🛠️ 빌드 시스템 (PlatformIO CLI), 리소스 사용 (v0410 최종), 필수 명령어

### Community 48 - "📐 명명 규칙 & 코드 정책"
Cohesion: 0.50
Nodes (4): ArduinoJson v7 정책, 📐 명명 규칙 & 코드 정책, 문자열/초기화, 안전 IO

### Community 49 - "📎 부록: 주요 상수"
Cohesion: 0.50
Nodes (4): Button Timings 기본값, Motion Advanced 기본값, Power Management 기본값, 📎 부록: 주요 상수

### Community 50 - "🔌 하드웨어"
Cohesion: 0.50
Nodes (4): MPU6050 설정, 물리 배치, 좌표계 규약 (필수), 🔌 하드웨어

### Community 51 - "14. Phase P-A 구현 명세 및 헤더 초안"
Cohesion: 0.50
Nodes (4): 14-1. `src/v0412/C20_Action_0412.h` 패치 (MACRO kind 추가), 14-2. `src/v0412/C10_Def_0412.h`, 14-3. `src/v0412/C10_Config_0412.h`, 14. Phase P-A 구현 명세 및 헤더 초안

### Community 52 - "16-1. P-J-a — 매크로 실행 중 Status 피드백 (`macro_running`)"
Cohesion: 0.50
Nodes (4): 16-1. P-J-a — 매크로 실행 중 Status 피드백 (`macro_running`), 1) 문제점 및 현상, 2) 해결 방안 비교, 3) 권장 구현 설계 (Option A)

### Community 53 - "16-6. v0412+ — Profile × Multi-Host Peer 자동 연동"
Cohesion: 0.50
Nodes (4): 16-6. v0412+ — Profile × Multi-Host Peer 자동 연동, 1) 개념, 2) 연동 시 스키마 확장안, 3) 유보 사유

### Community 54 - "1\. 현재 프로젝트 상태 vs Option A 요구사항"
Cohesion: 0.50
Nodes (4): 1.1 이미 준비된 기반 ✅, 1.2 즉시 충돌/수정 필요 ⚠️, 1.3 아키텍처 정합성 🎯, 1\. 현재 프로젝트 상태 vs Option A 요구사항

### Community 55 - "4\. Critical 2차 수정 상세 코드"
Cohesion: 0.50
Nodes (4): 4-1. B20 상태 머신 통합 (C-7), 4-2. tick 통합 처리 (C-7, C-9), 4-3. 콜백 reject (C-5, C-6 통합), 4\. Critical 2차 수정 상세 코드

### Community 56 - "2. 핵심 메커니즘 및 아키텍처 원칙 (Core Architecture)"
Cohesion: 0.50
Nodes (4): 2.1 Reject-Until-Target 시퀀스 흐름, 2.2 7대 핵심 아키텍처 결정 (1·2차 검토 통합 반영), 2.3 전력 관리 및 Sleep 방지 정책 (P20 연동), 2. 핵심 메커니즘 및 아키텍처 원칙 (Core Architecture)

### Community 57 - "_clamp"
Cohesion: 0.50
Nodes (4): _clamp(), T, CL_E10_EliteAirMouse::_applyE10ToRuntime(), U

### Community 58 - "ST_E10_PrecProfile_t"
Cohesion: 0.50
Nodes (4): ST_E10_PrecProfile_t, accel_limit, alpha, gain

### Community 59 - "_kbUsageLabel"
Cohesion: 0.50
Nodes (4): _kbUsageGroup(), _kbUsageLabel(), populateKbUsageSelect(), 2.19 W10 Consumer/KB 라벨 SSOT 계약 (rev13, B-1/B-3)

### Community 60 - "4. 데이터 스키마 및 영속성 설계 (Schema & Persistence)"
Cohesion: 0.50
Nodes (4): 4.1 신규 설정 파일: `/json/hosts_0415.json`, 4.2 부팅 상태 확장: `/json/boot_state_0415.json`, 4.3 본딩 NVS FIFO 런타임 동기화 (M-1) 및 Factory Reset (M-2), 4. 데이터 스키마 및 영속성 설계 (Schema & Persistence)

### Community 61 - "🚀 빌드"
Cohesion: 0.67
Nodes (3): platformio.ini, 리소스 (v0415 rev18 실측, BUDGET §7 정합), 🚀 빌드

### Community 62 - "CL_E10_EliteAirMouse::begin"
Cohesion: 0.67
Nodes (3): _applyFromConfig, _diagAndRecoverMpu, CL_E10_EliteAirMouse::begin()

### Community 63 - "10. 충전 및 절전 모드"
Cohesion: 0.50
Nodes (4): 10.1 충전, 10.2 2단계 지능형 자동 절전 시스템, 10.3 사용 중 오작동 방지, 10. 충전 및 절전 모드

### Community 82 - "5\. 프로젝트 적용 권장 순서"
Cohesion: 0.67
Nodes (3): 5\. 프로젝트 적용 권장 순서, 🎯 v0415.2 마이너 릴리스 (P0), ⚠️ 반드시 선행 검증

### Community 111 - "D10_Logger_0415.h"
Cohesion: 0.40
Nodes (3): Stream, stdarg, stdio

### Community 220 - "main.cpp"
Cohesion: 0.08
Nodes (10): arduinojson, esp_system, fs, littlefs, _w10_applyRuntimeE10(), _w10_getProfileInfo(), _w10_getStatus(), _w10_saveProfile() (+2 more)

### Community 283 - "LD2420"
Cohesion: 0.05
Nodes (41): EventBits_t, EventGroupHandle_t, LD2420CmdState, LD2420State, 5. 자동 캘리브레이션 API, LD2420DataCallback, LD2420EventCallback, LD2420CalState (+33 more)

### Community 332 - "📝 AirMouse Elite S3 (v0410) 종합 가이드 & 기술 사양서"
Cohesion: 0.05
Nodes (41): 3.1. FreeRTOS 태스크 모델, 3.2. 데이터 흐름 및 파이프라인, 3.3. 상태 소유권 및 동기화 매트릭스, 3.4. Mutex 동기화 정책, 3. 🏗️ 시스템 아키텍처, 1. 📌 AI 어시스턴트를 위한 핵심 요약 (Read First), 2. 🎯 프로젝트 개요 & 핵심 특징, 3.1. FreeRTOS 태스크 모델 (+33 more)

### Community 340 - "버튼·기능 설계 — 요구사항 및 용어 표준화 사양서"
Cohesion: 0.05
Nodes (36): 10. 남은 결정 항목 (매핑 확정 이전), 1. IMU 물리 신호 4종 (원천), 1. 물리 자산 명세, 2. 기능별 자연 소스 (핵심 표), 2. 파지 자세 정의 및 좌표계 불변식, 3.1 마우스 포인터 이동, 3.2 스크롤 휠 (수직), 3.3 스크롤 팬 (수평) (+28 more)

### Community 341 - "0415_변경_001.md"
Cohesion: 0.05
Nodes (39): 1\. API 엔드포인트 미사용 (백엔드 등록 ↔ 프론트 호출 0), 2.1 Precision Advanced 11개 필드, 2.2 Gyro Bias 3개 필드, 2\. /api/control 커맨드 미사용, 2\. Precision 세부 11개 필드 미노출 (🔴 High), 3.1 Linear / Tilt Hold, 3\. Gyro Bias Tracker 3개 필드 미노출 (🟡 Medium), 4.1 Linear (임펄스 적분) 3개 (+31 more)

### Community 416 - "[v0415] Multi-Host 전환 기능(Option A: Reject-Until-Target) 통합 상세 설계서"
Cohesion: 0.25
Nodes (7): 10.1 단계별 소요 공수, 10.2 즉시 착수 단계 (Immediate Action Item), 10. 공수 재산정 및 차기 과제 (Effort Estimation & Next Steps), 3. 기기 및 OS별 특이사항 및 대응 전략 (OS Compatibility Matrix), 8. 통합 검증 매트릭스 (Test Matrix), 9. 리스크 분석 및 대응 방안 (Risk & Mitigation), [v0415] Multi-Host 전환 기능(Option A: Reject-Until-Target) 통합 상세 설계서

### Community 431 - "C10_Config_Valid_0415.cpp"
Cohesion: 0.15
Nodes (13): validateE10, validateMacros, validateSlot, validateSlots, validateWiFi, CL_C10_Config::validateE10(), CL_C10_Config::validateProfile(), CL_C10_Config::validateSlots() (+5 more)

### Community 436 - "CL_C10_Config::patchProfileFromJson"
Cohesion: 0.33
Nodes (6): _patchE10Json, _patchMacrosJson, _patchSlotsJson, _patchWiFiJson, CL_C10_Config::patchProfileFromJson(), String

### Community 445 - "CL_E10_EliteAirMouse"
Cohesion: 0.01
Nodes (143): EN_C20_ReqHostOp_t, EN_E10_FreezeState_t, EN_E10_SnapAxis_t, CL_E10_EliteAirMouse, _accelGain, _accelTh, _accumDx, _accumDy (+135 more)

### Community 494 - "🤖 Agent Guide & Project Instructions (agent.md)"
Cohesion: 0.20
Nodes (9): 2. 📂 활성 소스 코드 및 디렉터리 체계, 5. 🧵 멀티태스킹 & 동기화 아키텍처 (Concurrency Rules), 6. 🚨 계약 기반 개발 및 리뷰 프로토콜 (Contract-Based Protocol), 7. 🔄 버전 업그레이드 시 동기화 체크리스트 (Version Bump Protocol), 8. 🚀 v0415 릴리스 상태 요약 (Current Status), 🤖 Agent Guide & Project Instructions (agent.md), ⚠️ 동기화 필수 준수 사항, 📋 일관된 리뷰 및 검토 출력 포맷 (Standard Review Response Format) (+1 more)

### Community 507 - "0415_변경_002.md"
Cohesion: 0.08
Nodes (25): 1\. 1차 검토서 평가, 3\. Critical 6건 정정 요약 (이전 검토 반영), 4\. hosts\_0415.json 스키마, 4\. hosts\_0415.json 스키마, 4\. 통합 공수 재산정, 5\. 최종 공수 재산정, 5\. 테스트 매트릭스, 5\. 테스트 매트릭스 (m-5) (+17 more)

### Community 589 - "📊 파워포인트 프레젠테이션 모드 단축키 요약"
Cohesion: 0.29
Nodes (6): 1. 슬라이드 쇼 시작 및 종료, 2. 슬라이드 이동 및 화면 제어, 3. 포인터 및 그리기 도구 (판서), 4. 유용한 추가 기능, 에어마우스 구현대상기능, 📊 파워포인트 프레젠테이션 모드 단축키 요약

### Community 596 - "CL_W10_WebConfig"
Cohesion: 0.04
Nodes (70): AsyncWebServer, CL_W10_WebConfig, apiActionTest, apiActionTestMacro, apiControl, _apiDiag, _apiDiagClear, apiExport (+62 more)

### Community 686 - "🚨 Graph-First Navigation Invariant (Strict Rule)"
Cohesion: 0.40
Nodes (4): 📌 2단계 필수 탐색 프로토콜 (Two-Step Navigation Protocol), 🚨 Graph-First Navigation Invariant (Strict Rule), 🔄 그래프 최신화 규칙, 🚫 절대 금지 사항

### Community 699 - "pio_gzip_0415.py"
Cohesion: 0.17
Nodes (18): gzip, os, re, scons_script, shutil, clean_dst_www(), copy_file(), create_js_bundle() (+10 more)

### Community 742 - "._pushFrame"
Cohesion: 0.10
Nodes (19): ⑥ 4대 절대 금지 계약 규칙 (CONTRACT Invariants), Contract-Based Review & Development Rules, 📋 일관된 리뷰 및 검토 출력 포맷, 🚨 필수 검토 및 준수 6대 원칙 (Invariants), CLAUDE.md — Claude Code Project Guidelines, 📌 Single Source of Truth: agent.md 최우선 참조 안내, 🚨 핵심 준수 4대 헌장 (Core Invariants Quick Reference), 4.1. E10 모듈 7파일 분할 기준 (+11 more)

### Community 753 - "LD2420.cpp"
Cohesion: 0.11
Nodes (30): HardwareSerial, LD2420Mode, TickType_t, applyCalibration, begin, cancelCalibration, cmdTaskTrampoline, emitEvent (+22 more)

### Community 771 - "CL_C10_Config"
Cohesion: 0.10
Nodes (42): esp_reset_reason_t, _C10_defaultProfileName(), CL_C10_Config, _atomicWriteJson, begin, _boot, bootMarkOkIfGracePassed, _bootMutex (+34 more)

### Community 783 - "16. 자주 묻는 문제 및 해결책"
Cohesion: 0.07
Nodes (28): 16. 자주 묻는 문제 및 해결책, (rev15 신규) Wi-Fi 설정을 변경했는데 언제 적용되나요?, (rev15 신규) "휘두르기 감도"를 조정해도 반응이 없었는데 v0415에서 달라졌나요?, (rev16 신규) 호스트 전환 시도 중 다른 기기가 먼저 연결되면 어떻게 되나요?, (rev16 신규) 호스트 전환이 실패했는지 어떻게 알 수 있나요?, (rev16 신규) 호스트에 별명을 지정할 수 있나요?, (v0415 신규) 기기를 v0412에서 v0415로 업데이트했더니 설정이 바뀐 것 같습니다, (v0415 신규) 스마트 TV 모드에서 전원 키가 TV를 켜지 못합니다 (+20 more)

### Community 784 - "CL_L10_Led"
Cohesion: 0.08
Nodes (47): EN_L10_State_t, CL_L10_Led, _apply, _applyBase, _baseColor, begin, blink, _blinkOn (+39 more)

### Community 785 - "ST_C10_HostsConfig_t"
Cohesion: 0.20
Nodes (11): ST_C10_Host_t, addr, color, label, order, pinned, ST_C10_HostsConfig_t, count (+3 more)

### Community 787 - "17. 자주 묻는 문제 (FAQ)"
Cohesion: 0.07
Nodes (28): 17. 자주 묻는 문제 (FAQ), Side C 2초와 3초가 어떻게 달라요?, TV 전원이 안 켜져요, TV 채널 조절이 안 돼요, WiFi 설정을 바꿨는데 언제 적용돼요?, 기기 전환 실패는 어떻게 알 수 있나요?, 기기 전환 중에 다른 기기가 먼저 연결되면 어떻게 되나요?, 기기에 별명을 붙일 수 있나요? (+20 more)

### Community 802 - "ST_E10_Status_t"
Cohesion: 0.04
Nodes (47): ST_E10_Status_t, active_mode, ble_connected, ble_peer_name, comm_dt_avg_ms, comm_dt_max_ms, comm_overrun_count, consecutive_fail (+39 more)

### Community 819 - "CL_A40_MutexGuard_Semaphore"
Cohesion: 0.24
Nodes (6): CL_A40_MutexGuard_Semaphore, _acquired, _caller, _mutexPtr, SemaphoreHandle_t, TickType_t

### Community 822 - "W10_Def_0415.h"
Cohesion: 0.07
Nodes (39): ST_W10_Consumer_t, mask, name, ST_W10_DiagEvt_t, code, ms, ST_W10_ErrMap_t, code (+31 more)

### Community 841 - "향후 개선 (선택, v0416 후보)"
Cohesion: 0.14
Nodes (14): §10 리스크 & 대응, §11 구현 공수, §12 스펙 반영 항목, §13 최종 요약, §1 개요, §2 핵심 메커니즘 (Reject-until-target), §3 기술적 제약, §4 OS/기기별 상세 (+6 more)

### Community 855 - "LD2420_Proto_001.cpp"
Cohesion: 0.22
Nodes (23): cmdTaskLoop, handleByte, handleCommandFrame, handleEnergyFrame, handleSimpleLine, LD2420Mode, LD2420Protocol, buildDisableConfig (+15 more)

### Community 867 - "7.10. 포인팅 정밀도 & 모션 알고리즘 고도화 (v0411)"
Cohesion: 0.20
Nodes (10): 7.10.10. 실측 검증 계획 (Phase 0, 사후 진행), 7.10.1. 모션 처리 파이프라인 (단방향, 6단계), 7.10.2. Adaptive Variable EMA (Phase 2), 7.10.3. Snap-to-Axis (Phase 3, Mode 2 프리셋 기본 ON), 7.10.4. Click-Freeze (Phase 1, 4-State FSM), 7.10.5. Config 스키마 확장, 7.10.6. Mode별 프리셋, 7.10.8. 단계별 배포 전략 (+2 more)

### Community 895 - "2\. 적용 단계별 계획 (4 Stage)"
Cohesion: 0.12
Nodes (17): 1-1. 신규 파일 C10\_Hosts\_0415.h (or C10\_Config 확장), 1-2. CL\_C10\_Config 확장 (기존 atomic rotate 재사용), 1-3. B20::begin() RPA 설정 보강, 2-1. B20\_Ble\_0415.h 확장, 2-2. 콜백 (기존 콜백과 체이닝), 2-3. tick에 타임아웃 검사 추가, 2\. 적용 단계별 계획 (4 Stage), 3-1. C20\_Action\_0415.h (+9 more)

### Community 897 - "Backend features to check for frontend gaps:"
Cohesion: 0.15
Nodes (13): 10\. Reset reason:, 11\. I2C recovery diagnostics in diag:, 12\. Backend OTA status fields:, 13\. /api/safeboot GET not used:, 14\. /api/reboot/check GET:, 1\. API Endpoints not used by Frontend:, 2\. Control Commands not used by Frontend:, 3\. E10 Callbacks not consumed by Frontend: (+5 more)

### Community 942 - "library.json"
Cohesion: 0.09
Nodes (21): authors, build, flags, libLDFMode, unflags, dependencies, espressif32, description (+13 more)

### Community 948 - "Elite AirMouse S3 — v0415 기술 사양서 (정본)"
Cohesion: 0.17
Nodes (11): `/api/control` cmd (v0415), `/api/status` 응답 확장 (v0415 rev16), Elite AirMouse S3 — v0415 기술 사양서 (정본), 🌀 Front Hold 스크롤, v0415 주요 변경 (v0412 → v0415), 🌐 Web API, 🧠 Zero-rate Bias Tracking (M20), ⚙️ 물리 엔진 (M10) (+3 more)

### Community 953 - "Elite AirMouse S3 — 사용자 매뉴얼"
Cohesion: 0.13
Nodes (14): 17. 핵심 조작 요약표, 18. 안전 주의사항, 19. 부록: 버전별 주요 변경 이력, 1. 30초 요약 (처음 사용자용), 20. 사용자 지원, 2. 구성품 안내, 4. 버튼 배치 및 명칭, 8.1 모드 순환 (+6 more)

### Community 954 - "CL_C20_ActionExec"
Cohesion: 0.15
Nodes (24): CL_C20_ActionExec, _allocRuntime, begin, _consumerTapMs, exec, _execConsumerRep, _execConsumerTap, _execKbRepeat (+16 more)

### Community 962 - "CL_M20_BiasTracker"
Cohesion: 0.06
Nodes (19): CL_M20_BiasTracker, _alpha, _biasX, _biasY, _biasZ, _fastRecalibActive, _fastRecalibCount, _fastRecalibDurationMs (+11 more)

### Community 980 - "🔍 예제별 상세 설명"
Cohesion: 0.11
Nodes (17): 01. 기본 재실 감지 ([01_BasicPresence.ino](01_BasicPresence/01_BasicPresence.ino)), 02. 최대 게이트 거리 제한 ([02_MaxGateLimit.ino](02_MaxGateLimit/02_MaxGateLimit.ino)), 03. 특정 구역 필터링 ([03_ZoneDetection.ino](03_ZoneDetection/03_ZoneDetection.ino)), 04. 거리별 차등 감도 설정 ([04_TieredSensitivity.ino](04_TieredSensitivity/04_TieredSensitivity.ino)), 05. 스마트 욕실 조명 및 환풍기 제어 ([05_BathroomLightFan.ino](05_BathroomLightFan/05_BathroomLightFan.ino)), 06. 천장 선풍기 오탐 필터링 ([06_CeilingFanFilter.ino](06_CeilingFanFilter/06_CeilingFanFilter.ino)), 07. 완전 비동기 자동 캘리브레이션 ([07_AutoCalibration.ino](07_AutoCalibration/07_AutoCalibration.ino)), 08. 시리얼 대화형 실시간 수동 튜닝 ([08_ManualCalibration.ino](08_ManualCalibration/08_ManualCalibration.ino)) (+9 more)

### Community 983 - "LD2420Calibration"
Cohesion: 0.17
Nodes (11): LD2420CalState, LD2420Calibration, cancel, computeConfig, _lastSampleMs, LD2420Calibration::LD2420Calibration(), progress, reset (+3 more)

### Community 985 - ".isSelecting"
Cohesion: 0.12
Nodes (15): 4.1 B20 BLE Manager (`B20_Ble_0415`), 4.2 C10 Config Manager (`C10_Config_0415`), 4.3 E10 & C20 Task/Action 연동 (CONTRACT §3 준수), 4.4 W10 Web API 및 UI 모듈, 4. 모듈별 기본 설계, 3\. 1차 검토서 + 2차 통합 — 최종 수정 매트릭스, 7\. 최종 판정, 8\. 다음 단계 (+7 more)

### Community 987 - "API 레퍼런스"
Cohesion: 0.22
Nodes (8): 1. 생성자 및 수명주기 초기화, 3. 비동기 설정 명령 API (즉시 반환, Non-blocking), 4. 감도 프리셋 편의 함수, 6. 콜백 등록 API, API 레퍼런스, LD2420GateConfig, move_threshold, still_threshold

### Community 988 - "LD2420 Arduino Library"
Cohesion: 0.13
Nodes (15): Arduino IDE, FAQ, LD2420 Arduino Library, LD2420 펌웨어 호환성, PlatformIO, 게이트 시스템, 라이선스 및 참고 자료, 목차 (+7 more)

### Community 989 - "LD2420.h"
Cohesion: 0.13
Nodes (13): event_groups, freertos, HardwareSerial, LD2420CmdFrame, command, data, data_length, LD2420VersionInfo (+5 more)

### Community 994 - "LD2420TargetData"
Cohesion: 0.14
Nodes (12): 📋 예제 목록 요약, 2. 상태 조회 API (논블로킹, 즉시 반환), feed, _dataCb, getLatestData, publishData, LD2420TargetData, distance_cm (+4 more)

### Community 1002 - "[0.0.1] - 2026-10-06"
Cohesion: 0.33
Nodes (5): [0.0.1] - 2026-10-06, Added, Changelog, Compatibility, Known Issues

### Community 1005 - "CL_M10_AdvancedMotionProcessor"
Cohesion: 0.06
Nodes (25): CL_M10_AdvancedMotionProcessor, _computeEmaAlpha, _dpiGain, _emaCfg, _emaInitDone, G_COMP_ALPHA, G_M10_DPI_BASE, G_M10_DPI_STEP (+17 more)

### Community 1006 - "CL_M30_Gesture"
Cohesion: 0.10
Nodes (30): CL_M30_Gesture, _cfg, _detectFlickRoll, _detectFlickYaw, _detectLinear, _detectTilt, _flick, _flickPeakToPeak (+22 more)

### Community 1007 - "ST_C10_E10Config_t"
Cohesion: 0.06
Nodes (32): ST_C10_E10Config_t, accel_gain, accel_threshold, active_mode, active_peer_index, battery_adc_enabled, button, dpi_level (+24 more)

### Community 1008 - "am_base_0415.js"
Cohesion: 0.10
Nodes (5): applyTheme(), g_triggers, initTheme(), toast(), toggleTheme()

### Community 1009 - "W10_Web_Static_0415.cpp"
Cohesion: 0.10
Nodes (21): ST_W10_BodySlot, buf, lastMs, len, req, _bodyFree, _bodyGetSlot, _bodySlotAlloc (+13 more)

### Community 1010 - "AsyncWebServerRequest"
Cohesion: 0.13
Nodes (43): AsyncWebServerRequest, ST_W10_E10If_t, ctx, _bumpRebootMaskFromBoot, _collectBodyOrReply, _diagPush, _gateSafeModeOrReply, _isSafeMode (+35 more)

### Community 1013 - "CL_D10_Logger"
Cohesion: 0.12
Nodes (28): CL_D10_Logger, begin, BUFFER_SIZE, clear, _getColor, getCorruptCount, getLevel, getOverwriteCount (+20 more)

### Community 1015 - "A40_ComFunc_0415.h"
Cohesion: 0.28
Nodes (15): memory, new, shared_ptr, _A40__callerOrUnknown(), _buildPathWithSuffix(), cloneStr2SharedStr_safe(), Copy_File_V22(), copyStr2Buffer_safe() (+7 more)

### Community 1017 - "FLOW_0415.md — 기능별 데이터 흐름 및 파이프라인 명세"
Cohesion: 0.04
Nodes (44): 10. `_markNeedReboot` 되돌림 감지 흐름 (W10), 11. MPU6050 I2C 진단, 주소 자동 감지 및 런타임 복구 흐름, 12.1 [v0415 rev15] Wi-Fi 웹 UI 저장 흐름, 12. Wi-Fi SoftAP 즉시 구동, 웹 라우팅 별칭 및 SafeBoot 분기 흐름, 13. D10 Logger 2단계 락(Two-Phase Locking) 및 Serial 단일 버퍼 파이프라인, 14. C10 Config 안전 회전(Atomic Rotate Save) 파이프라인, 15. LED 태스크 스택 관측 파이프라인 (rev13 신규, B-2), 17. 개정 이력 (+36 more)

### Community 1019 - "E10_AirMouse_Action_0415.cpp"
Cohesion: 0.12
Nodes (18): 9. 위반 시 조치, 하드코딩 규칙 (`_handleHardcodedButton`), _enqueueAction, _handleSpecial, requestGyroCalibration, _resolveSlot, CL_E10_EliteAirMouse::_enqueueAction(), CL_E10_EliteAirMouse::_handleGesture() (+10 more)

### Community 1020 - "📝 AirMouse Elite S3 (v0415) 종합 가이드 & 기술 사양서"
Cohesion: 0.15
Nodes (12): 1. 📌 AI 어시스턴트를 위한 핵심 요약 (Read First), 2. 🎯 프로젝트 개요 & 핵심 특징, 4.0. 백엔드 (`src/v0415/`), 4.2. 프론트엔드 8-모듈 구조 (`data_v0415_www/`), 4. 📁 프로젝트 디렉터리 구조, 8. 🖥️ Web UI (v0415) 프론트엔드 & i18n 표준화 가이드, 9. 🔌 하드웨어 사양 및 핀맵, 📝 AirMouse Elite S3 (v0415) 종합 가이드 & 기술 사양서 (+4 more)

### Community 1021 - "E10_AirMouse_Core_0415.cpp"
Cohesion: 0.13
Nodes (10): _applyE10ToRuntime, _applyRuntimeLocked, _snapshotRuntimeToE10Config, CL_E10_EliteAirMouse::applyRuntimeE10(), CL_E10_EliteAirMouse::_applyRuntimeLocked(), CL_E10_EliteAirMouse::_reloadActiveProfile(), CL_E10_EliteAirMouse::_saveActiveProfile(), CL_E10_EliteAirMouse::setDpiLevel() (+2 more)

### Community 1022 - "CL_P20_Power"
Cohesion: 0.07
Nodes (37): esp_sleep, gpio, rtc_io, CL_P20_Power, _armExt1, begin, _cfg, deepSleepNow (+29 more)

### Community 1023 - "W10_WebApi_Com_0415.cpp"
Cohesion: 0.13
Nodes (12): _httpFromCode, _sendJsonStream, CL_W10_WebConfig::_ifNoneMatchHit(), CL_W10_WebConfig::_isApiAllowedInSafeMode(), CL_W10_WebConfig::_rebootReasonsString(), CL_W10_WebConfig::_sendErr(), CL_W10_WebConfig::_sendJsonStream(), CL_W10_WebConfig::_sendOk() (+4 more)

### Community 1024 - "buildWakeMaskSafe"
Cohesion: 0.26
Nodes (11): 1. 📌 핵심 프로젝트 개요, 10. 개정 이력, v0415 Safe/Pairing Wake Mask 분기, 📌 AI 어시스턴트용 컨텍스트 (Read First), Deep-sleep 조건 및 동작, GPIO 배정 (`HW_Def_0415.h` SSOT), v0415 (2026-10-08 ~ 10-09), 2.7 Button Dispatcher Phase 전이 (+3 more)

### Community 1025 - "CL_B20_Ble"
Cohesion: 0.05
Nodes (50): EN_B20_AdvMode_t, nimbledevice, CL_B20_Ble, _activePeerIndex, _advMode, _advModeStartMs, _advModeTimeoutMs, begin (+42 more)

### Community 1026 - "ST_C20_ActionSlot_t"
Cohesion: 0.16
Nodes (22): makeDefaultsSlots, C20_ComboMod(), C20_ComboU1(), C20_ComboU2(), C20_ComboU3(), C20_EncodeCombo(), C20_EncodeWheel(), C20_MakeConsumer() (+14 more)

### Community 1027 - "14. 포인터 및 센서 세부 설정 (Config 탭)"
Cohesion: 0.15
Nodes (13): 14.1 기본 파라미터, 14.2 동작 정밀도 (Motion Advanced) 설정, 14.3 절전 및 전원 관리 설정, 14.4 버튼 타이밍 설정, 14.5 공장 초기화, 14.6 Wi-Fi 설정 (rev15 신규), 14. 포인터 및 센서 세부 설정 (Config 탭), 발표용 축 스냅 (Snap-to-Axis) (+5 more)

### Community 1028 - "am_status_0415.js"
Cohesion: 0.17
Nodes (17): bindTroubleshoot(), ctlGyroCalibWithFeedback(), ctlI2cRecoverWithFeedback(), g_recentLogs, _getE10Bias(), _getE10GyroRms(), handleTrouble(), keyTest() (+9 more)

### Community 1029 - "Elite AirMouse S3 — v0412 백엔드 논리/크리티컬 결함 점검 및 개선 요구서"
Cohesion: 0.18
Nodes (10): 1. 개요, 3. 개선 요구사항 명세 (Action Items), 5. 단계별 적용 및 검증 완료 결과, 6.1 추가 결함 발견 및 원인 분석, 6.2 2차 빌드 및 산출물 검증 결과, 6. 2차 점검: 심층 논리 모순 분석 및 추가 수정 (완료 ✅), 7.1 결함 내용 및 메커니즘, 7.2 3차 빌드 및 산출물 검증 결과 (+2 more)

### Community 1030 - "C10_Config_Json_0415.cpp"
Cohesion: 0.13
Nodes (22): JsonVariantConst, _buildMacroStepJson, _buildSlotArrayJson, _patchMacroJson, _patchMacroStepJson, _patchSlotArrayJson, _patchSlotJson, CL_C10_Config::_buildE10Json() (+14 more)

### Community 1031 - "W10_WebApi_Status_0415.cpp"
Cohesion: 0.15
Nodes (12): C10_TriggerGroup(), C10_TriggerName(), 명명 규칙 (엄격), _calcFileEtag32, _fillE10StatusFromSnapshot, _precModeName, _rebootReasonsString, CL_W10_WebConfig::apiKeycodes() (+4 more)

### Community 1032 - "2. 상태머신(FSM) 상세 명세"
Cohesion: 0.12
Nodes (15): 1.1 v0415 주요 변경, 1. 상태 변수 소유권 매트릭스, 2.10 C10 Boot State FSM (Mutex 보호), 2.1 Click-Freeze FSM (`_applyClickFreeze`), 2.2 Snap-to-Axis FSM (`_applySnapToAxis`), 2.3 Precision FSM (`_fsmUpdate` / `_applyPrecision`), 2.4 Macro Sequencer FSM (`_tickMacro`), 2.5 Macro 대체(Replace) 및 취소(Abort) 정책 (+7 more)

### Community 1033 - "16. v0412 잔여 유보 이슈 상세 및 대응 방안 (Deferred Issues)"
Cohesion: 0.12
Nodes (16): 16-2. P-J-b — 매크로 소요 시간(Duration) UI 표시, 16-3. P-J-c — 매크로 편집 Dirty 상태 감지 및 변경 유실 방지, 16-4. P-J-d — 슬롯 편집 중 프로파일 전환 시 데이터 안전 보존, 16-5. P-J-e — 물리 버튼 조합 기반 프로파일 전환 트리거, 16-7. 잔여 유보 이슈 우선순위 매트릭스, 16-8. 후속 구현 선택 옵션, 16. v0412 잔여 유보 이슈 상세 및 대응 방안 (Deferred Issues), 1) 문제점 (+8 more)

### Community 1034 - "am_profile_0415.js"
Cohesion: 0.21
Nodes (16): 10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11), getOverrideMask(), getSlotsArray(), profileCreate(), profileDelete(), profileReloadAll(), profileRename(), profileSwitch() (+8 more)

### Community 1035 - "am_remote_0415.js"
Cohesion: 0.18
Nodes (16): applyRemoteMode(), bindModeTabs(), bindRemoteFoldToggle(), ctlSetMode(), initHybridJoystick(), getDir(), handleMove(), handleStart() (+8 more)

### Community 1037 - "6. v0415 리팩터 반영"
Cohesion: 0.20
Nodes (10): 6.11 Deep-sleep 셧다운 예산 (v0415 rev9 신규), 6.1 L10 LED mutex (Round F), 6.2 P20 Safe/Pairing Wake Mask (Round D/E), 6.3 C10 스키마 및 마이그레이션 예산 (Phase 5-A 개정), 6.4 D10 대폭 축소 (Round L), 6.6 I2C 버스 복구 및 센서 진단 예산 (v0415), 6.7 Wi-Fi 부팅 시간 최적화 (v0415), 6.8 웹 소켓 풀 보존 및 API 메모리 절감 예산 (Phase 5-B 개정) (+2 more)

### Community 1038 - "백엔드 ↔ 프론트엔드 상수 공유 방안 검토"
Cohesion: 0.17
Nodes (11): 3.1 생성 파일 구조, 3.2 pio_gzip_0415.py 확장, 3.3 프론트 로드 순서 변경, 3.4 버전 검증 (선택), 3. 권장 방안 — B + A 하이브리드, 5. 리스크 & 대응, 6. 대안 검토 — "API만으로 충분하지 않나?", 7. 프로젝트 철학 정합성 (+3 more)

### Community 1039 - "웹 기반 사용자 개별화 고도화 — 통합 구현 계획서 (v0412)"
Cohesion: 0.15
Nodes (12): 0. 관련 요구사항 문서, 10. 마이그레이션 및 기존 Config 처리 정책 (D6, R10), 11. 시스템 자원 예산 분석, 12. 전체 개발 Phase 분할, 13. 핵심 설계 결정 사항 (R1~R10 확정), 1. 요구사항 정리 및 결정 요약, 4-1. 인덱스 매핑 및 잠금 정의, 4. 트리거 라이브러리 및 잠금 정책 (27개) (+4 more)

### Community 1041 - "2. 함수 인터페이스 계약"
Cohesion: 0.12
Nodes (17): 2.11 I2C 버스 복구 및 센서 주소 자동 탐색 계약 (v0415), 2.12 NimBLE 비동기 초기화 및 Mutex 방어 계약 (v0415), 2.13 Wi-Fi SoftAP 전환, 웹 라우팅 별칭 및 SafeBoot 버퍼 무결성 계약 (v0415), 2.14 Deep-sleep 셧다운 순서 계약 (v0415 rev12 신규), 2.15 C10 부팅 상태 Mutex 계약 (v0415 rev12 신규), 2.16 C10 원자적 저장 rotate 계약 (v0415 rev12 신규), 2.17 W10 apiExport 파일명 Sanitize 계약 (v0415 rev12 신규), 2.1 하드웨어 핀 및 Wake Mask SSOT 계약 (`HW_Def_0415.h`) (+9 more)

### Community 1042 - "ST_C10_Macro_t"
Cohesion: 0.14
Nodes (14): validateMacroStep, CL_C10_Config::validateMacro(), ST_C10_Macro_t, name, _pad, stepCount, steps, ST_C10_MacroStep_t (+6 more)

### Community 1043 - "Part 3: v0412 핵심 기술 사양 요약 (Summary Specification)"
Cohesion: 0.14
Nodes (14): 10. 소스 파일 구조 (`v0412`), 2-1. LittleFS 파일 구조, 2-2. 활성 프로파일 메타데이터 (`active_profile.json`), 2-3. 매크로 제약 사양, 2. Config 스키마 v5 및 스토리지, 3. 트리거 라이브러리 및 잠금 정책 (27개), 4. Action Registry (v0412), 6-1. 탭 네비게이션 (총 6개 메인 탭) (+6 more)

### Community 1044 - "E10_AirMouse_Diag_0415.cpp"
Cohesion: 0.18
Nodes (5): _pushErr, CL_E10_EliteAirMouse::setOtaGuard(), CL_E10_EliteAirMouse::setSafeMode(), CL_E10_EliteAirMouse::_recoverI2C(), CL_E10_EliteAirMouse::_runGyroCalibration()

### Community 1045 - "🎛️ Mode 시스템"
Cohesion: 0.29
Nodes (7): 27개 고정 트리거 인덱스 매핑, 2단 슬롯 매트릭스 (Global + Mode Override), 3 Modes, 🎛️ Mode 시스템, 버튼 디스패처 (C20), 설정 구조체 (`ST_C10_ButtonConfig_t`), 핵심 알고리즘

### Community 1046 - "ST_C10_ProfileConfig_t"
Cohesion: 0.14
Nodes (14): _buildE10Json, _buildMacrosJson, _buildSlotsJson, _buildWiFiJson, CL_C10_Config::buildProfileJson(), JsonDocument, ST_C10_ProfileConfig_t, e10 (+6 more)

### Community 1047 - "10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11)"
Cohesion: 0.26
Nodes (11): 10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11), 7.10.7. 모듈 책임 분리 (Architecture Boundary), applyMotionPreset(), cfgImport(), cfgLoad(), cfgSave(), configToUi(), uiToConfig() (+3 more)

### Community 1048 - "docs_v0415/Req_ImplPlan/30.impl_0410_002.md"
Cohesion: 0.14
Nodes (13): 버튼 디바운스(C20_BtnDispatcher) 점검 및 개선, 버튼 디바운스 Phase 1 + Phase 2 완전 구현, 전원 관리 개선 구현, 항목 LOC, 항목 LOC, 항목 LOC, 항목 LOC, 항목 LOC (+5 more)

### Community 1050 - "E10_AirMouse_Hid_0415.cpp"
Cohesion: 0.11
Nodes (15): _doReleaseAllButtons, _enqueueHidCmd, _handleCommDisconnectEdge, _handleCommGate, _sendPptKey2, _startMacro, _tapComboUsageKb, _tapConsumerMask (+7 more)

### Community 1051 - "2. 도메인별 표준 용어 매핑 테이블"
Cohesion: 0.12
Nodes (16): 1. 3대 핵심 표준화 원칙, 2. 도메인별 표준 용어 매핑 테이블, 3.1 [rev15] gesture/Wi-Fi 정합, 3.2 [누적] v0414 → v0415 리팩터, 3. v0415 변경 사항 (v0414 → v0415), 4. 개발자 준수 가이드라인, 5. v0415 새 용어 (TV 모드 Descriptor 제약 반영), A. 시스템 동작 모드 (Operating Modes) (+8 more)

### Community 1052 - "7. 🚀 v0415 핵심 기능 상세 사양"
Cohesion: 0.20
Nodes (10): 7.1. 3-Mode 시스템 (PC / PPT / TV), 7.2. 2단 슬롯 매트릭스 (Global + Mode Override), 7.3. 4대 필수 슬롯 잠금 (🔒), 7.4. 8×8 매크로 라이브러리 & 안전 취소 시퀀서, 7.5. M10 물리 모션 엔진 & M30 제스처, 7.6. Front Hold 스크롤, 7.7. SafeBoot & OTA Guard (method-aware), 7.8. 다중 프로파일 시스템 (Schema v5) (+2 more)

### Community 1053 - "W10_Web_init_0415.cpp"
Cohesion: 0.19
Nodes (9): _startAp, _startMdns, _stopMdns, CL_W10_WebConfig::_onWifiEvent(), CL_W10_WebConfig::s_wifiEvent(), CL_W10_WebConfig::_setupWiFi(), CL_W10_WebConfig::_startAp(), WiFiEvent_t (+1 more)

### Community 1057 - "ST_C10_BootState_t"
Cohesion: 0.25
Nodes (8): getBootState, ST_C10_BootState_t, boot_ms, fail_count, last_host_addr, last_reset_reason, pending, safe_mode

### Community 1058 - "ST_C10_MotionAdv_Ema_t"
Cohesion: 0.17
Nodes (12): ST_C10_MotionAdv_Ema_t, alpha_max, alpha_min, deadzone_th, fast_th, _pad, reversal_reset, reversal_th (+4 more)

### Community 1059 - "ST_C10_PowerConfig_t"
Cohesion: 0.17
Nodes (12): ST_C10_PowerConfig_t, deep_idle_timeout_ms, fast_recalib_ms, idle_timeout_ble_ms, idle_timeout_ms, led_fadein_ms, led_fadeout_ms, _pad (+4 more)

### Community 1060 - "15. 진단 및 입력 테스트 화면"
Cohesion: 0.18
Nodes (11): 15.10 진단 요약 카드 (Health), 15.1 시스템 현황, 15.2 이벤트 기록, 15.3 최근 명령 로그, 15.4 입력 신호 테스트 (Key Test), 15.5 자이로 영점 조절 피드백, 15.6 I2C 복구 피드백, 15.7 안전 부팅 (SafeBoot) 상태 (+3 more)

### Community 1061 - "사용자 개별화 기능 고도화 — 검토"
Cohesion: 0.18
Nodes (10): 1. 현재 개인화 수준 (실측), 2. 개별화 확장 축 (3개), 2차 (A 안정 후), 4. 자원 예산 재점검, 5. 추천 조합, 7. 결정 필요 항목, 8. 결론 및 다음 스텝, 결정 사항 (+2 more)

### Community 1062 - "웹 페이지 기반 사용자 개별화 — 고도화 검토"
Cohesion: 0.18
Nodes (11): 10. 비교 요약, 11. Mode별 vs Global (재확인), 12. 남은 결정 필요, 13. 다음 단계 후보, 1. 현재 웹 개별화 수준 (실측), 2. 개별화 3축 (Button / Gesture / Action), 5. 검증 정책 (개별화의 필수 동반), 6. Mode별 vs Global 개별화 (+3 more)

### Community 1063 - "웹 기반 사용자 개별화 고도화 — 통합 구현 계획서 (v0412)"
Cohesion: 0.04
Nodes (46): 0. 관련 요구사항 문서, 10. 마이그레이션 및 기존 Config 처리 정책 (D6, R10), 11. 시스템 자원 예산 분석, 12. 전체 개발 Phase 분할, 13. 핵심 설계 결정 사항 (R1~R10 확정), 14-1. `src/v0412/C20_Action_0412.h` 패치 (MACRO kind 추가), 14-2. `src/v0412/C10_Def_0412.h`, 14-3. `src/v0412/C10_Config_0412.h` (+38 more)

### Community 1065 - "ST_C10_ButtonConfig_t"
Cohesion: 0.15
Nodes (13): 7.12.1. 설정 파라미터 (`ST_C10_ButtonConfig_t`), 7.12.2. 핵심 알고리즘, 7.12. 버튼 디스패처 & 타이밍 고도화 (C20), ST_C10_ButtonConfig_t, debounce_min_ticks, debounce_press_ms, debounce_release_ms, double_delay_ms (+5 more)

### Community 1066 - "🟢 A. 프로파일 세트 (권장)"
Cohesion: 0.20
Nodes (10): 3. 각 축 상세, 🟢 A. 프로파일 세트 (권장), 🟢 B. 매크로 (액션 시퀀스), 🟡 C. 슬롯 프리셋 (스니펫), 개념:, 기능:, 리스크:, 비용: (+2 more)

### Community 1067 - "7.10. 포인팅 정밀도 & 모션 알고리즘 고도화 (v0412)"
Cohesion: 0.20
Nodes (9): 7.10.1. 모션 처리 파이프라인 (단방향, 6단계), 7.10.2. Adaptive Variable EMA, 7.10.3. Snap-to-Axis (Mode 2 프리셋 기본 ON), 7.10.4. Click-Freeze (4-State FSM), 7.10.5. Config 스키마 확장, 7.10.6. Mode별 프리셋, 7.10.7. 모듈 책임 분리, 7.10.8. 실측 검증 계획 (구현 전) (+1 more)

### Community 1071 - "ST_C10_MotionAdv_ClickFreeze_t"
Cohesion: 0.22
Nodes (9): ST_C10_MotionAdv_ClickFreeze_t, enable, fadeout_ms, freeze_move_th, gyro_th, hold_ms, max_ms, move_th (+1 more)

### Community 1072 - "app_0415_0001.js"
Cohesion: 0.40
Nodes (10): bindBanner(), bindDrawer(), bindQuickTuning(), bindUi(), closeDrawer(), _initHashRouting(), main(), openDrawer() (+2 more)

### Community 1073 - "am_macro_0415.js"
Cohesion: 0.49
Nodes (7): buildMacroStepParams(), getMacrosArray(), macroAdd(), macroReload(), macroRenderEditor(), macroRenderList(), macroSave()

### Community 1075 - "Elite AirMouse S3 — 사용자 매뉴얼"
Cohesion: 0.18
Nodes (10): 18. 핵심 조작 요약표, 19. 안전 주의사항, 1. 30초 요약 (처음 사용자용), 2.1 이 제품은 무엇인가요?, 2.2 어떤 기능이 있나요?, 2.3 안전한 충전과 보관, 2. 제품 소개, 4. 버튼 이름과 위치 (+2 more)

### Community 1076 - "13. 버튼 및 동작 맞춤 할당"
Cohesion: 0.22
Nodes (9): 13.1 슬롯이란?, 13.2 27가지 동작 분류, 13.3 절대 바꿀 수 없는 4개 동작 🔒, 13.4 "기본 공통" vs "모드별 재정의", 13.5 배정할 수 있는 10가지 동작 종류, 13.6 특수 기기 제어 (SPECIAL) 10가지, 13.7 미디어/리모컨 키 (Consumer) 24가지, 13.8 실시간 시험 및 저장 (+1 more)

### Community 1077 - "W10_Web_0415.h"
Cohesion: 0.25
Nodes (6): espasyncwebserver, espmdns, _wifiDiffMask, CL_W10_WebConfig::_bumpRebootMaskFromBoot(), update, wifi

### Community 1079 - "ST_C10_MotionAdv_Snap_t"
Cohesion: 0.25
Nodes (8): ST_C10_MotionAdv_Snap_t, axis_mode, confirm_frames, enable, mode_mask, _pad, ratio_enter, strength

### Community 1080 - "🛡️ SafeBoot / OTA Guard"
Cohesion: 0.29
Nodes (7): Bad Reset Reason, Boot State (`/json/boot_state_0415.json`), OTA Guard, 🛡️ SafeBoot / OTA Guard, SafeMode 정책 (method-aware), WiFi 브릭 방지, 절차

### Community 1083 - "📝 Config & Profile System (Schema 411)"
Cohesion: 0.29
Nodes (7): 📝 Config & Profile System (Schema 411), deleteProfile 정책 (v0415), LittleFS 저장 구조, loadProfile 정책 (Phase 5-A 개정), 검증 규칙, 프로파일 로드 및 스키마 정책 (Schema 412, rev18.1), 프로파일 본체 스키마

### Community 1085 - "CONTRACT_0415.md — 모듈 경계 및 인터페이스 계약"
Cohesion: 0.05
Nodes (39): 4. 큐 깊이 및 드롭 정책, 1. 개요 및 설계 원칙, 3. FreeRTOS 큐 계약, 4. 공유 변수 및 상태 동기화 계약, 4대 절대 금지 규칙, 5.1 SLEEP_NOW 계약 (v0415 Q4-a), 5. 태스크 간 Special 액션 라우팅 계약, 6.1 `_startMacro` 원자화 (v0415) (+31 more)

### Community 1086 - "am_host_0415.js"
Cohesion: 0.49
Nodes (9): hostCancel(), hostFetchList(), hostSaveLabel(), hostSelect(), initHostModule(), _openHostEditModal(), renderHostCard(), _startHostSelectingPoll() (+1 more)

### Community 1088 - "am_i18n_0415.js"
Cohesion: 0.48
Nodes (6): applyI18nToDom(), I18N_DICT, initI18n(), _setI18nText(), setLanguage(), t()

### Community 1091 - "6. A 도입 시 설계 요약"
Cohesion: 0.29
Nodes (7): 6-1. 파일 구조, 6-2. 전환 트리거, 6-3. 전환 시 안전 처리, 6-4. 웹 UI, 6-5. API 확장, 6-6. 공통 vs 프로파일별 필드, 6. A 도입 시 설계 요약

### Community 1092 - "Elite AirMouse S3 기능적 관점 보완 사항"
Cohesion: 0.29
Nodes (6): 1. 프레젠테이션 전용 기능 보완, 2. 연결성 및 호환성(Plug & Play) 보완, 3. 포인팅 정밀도 및 모션 알고리즘 고도화, 4. 사용자 인지 부하 감소 및 조작 안전성 보완, 5. 기능 보완 구현 로드맵 요약, Elite AirMouse S3 기능적 관점 보완 사항

### Community 1093 - "12. 버튼 및 동작 맞춤 할당 화면 (슬롯 관리)"
Cohesion: 0.25
Nodes (8): 12.1 개념 정의, 12.2 27가지 조작 동작 분류, 12.3 절대 수정 불가 잠금 동작 (오작동 방지용), 12.4 전체 공통 적용 vs 모드별 덮어쓰기, 12.5 지원되는 10가지 실행 동작 종류, 12.6 Consumer 키 목록 (v0415 갱신, 표준 명칭), 12.7 실시간 동작 확인 (Test) 및 저장, 12. 버튼 및 동작 맞춤 할당 화면 (슬롯 관리)

### Community 1094 - "15. 세부 설정 (포인터 및 센서)"
Cohesion: 0.25
Nodes (8): 15.1 기본 설정, 15.2 정밀 동작 설정 (일반 사용자는 그대로 두세요), 15.3 추천 프리셋 5종, 15.4 절전 설정, 15.5 버튼 타이밍 설정, 15.6 공장 초기화 (2가지 방법), 15.7 Wi-Fi 설정, 15. 세부 설정 (포인터 및 센서)

### Community 1095 - "16. 진단 및 문제 해결"
Cohesion: 0.25
Nodes (8): 16.1 시스템 현황 보기, 16.2 기기 건강 상태 카드, 16.3 문제 해결 도구, 16.4 입력 신호 테스트, 16.5 최근 명령 로그, 16.6 기기 전환 상태 조회, 16.7 무선 업데이트 (OTA), 16. 진단 및 문제 해결

### Community 1097 - "7. 모드 3: 스마트 TV 모드 (표시등: 주황색)"
Cohesion: 0.29
Nodes (7): 7.1 진입 방법, 7.2 스마트 TV 전용 버튼 매핑 (v0415 갱신), 7.3 TV 모드 기능 제약 안내 (v0415), 7.4 기울여 유지하기(틸트 홀드)를 통한 4방향 키 제어, 7.5 커서 방식 지원, 7.6 TV 공중 제스처, 7. 모드 3: 스마트 TV 모드 (표시등: 주황색)

### Community 1098 - "E10_AirMouse_Task_0415.cpp"
Cohesion: 0.12
Nodes (8): 전환 (고정), _doForceReleaseNow, _drainAllQueuesOnGate, forceReleaseButtons, CL_E10_EliteAirMouse::_setActiveMode(), CL_E10_EliteAirMouse::switchProfile(), CL_E10_EliteAirMouse::_handleCommDisconnectEdge(), CL_E10_EliteAirMouse::_handleCommGate()

### Community 1099 - "ST_MacroState_t"
Cohesion: 0.29
Nodes (7): ST_MacroState_t, active, macroIdx, _pad, startToken, stepIdx, stepStartMs

### Community 1102 - "11. 브라우저 기반 상세 설정 화면 (웹 UI)"
Cohesion: 0.33
Nodes (6): 11.1 접속 방식, 11.2 네비게이션 드로어 및 언어 전환 (KO / EN), 11.3 홈 탭의 필수 조작 카드 (rev16 신규), 11.4 홈 탭의 호스트 기기 전환 카드 (rev16 신규), 11.5 홈 탭의 하이브리드 가상 컨트롤러 (Virtual Remote), 11. 브라우저 기반 상세 설정 화면 (웹 UI)

### Community 1105 - "13. 자동 연속 동작 화면 (매크로 관리)"
Cohesion: 0.33
Nodes (6): 13.1 개념 정의, 13.2 시스템 제약 규격, 13.3 생성 및 편집 절차, 13.4 실생활 추천 활용 구성, 13.5 비상 중단 안전장치, 13. 자동 연속 동작 화면 (매크로 관리)

### Community 1106 - "5. 모드 1: PC 일반 작업 모드 (표시등: 파란색)"
Cohesion: 0.33
Nodes (6): 5.1 커서 조작 방식, 5.2 공장 초기 버튼 매핑, 5.3 화면 스크롤 및 가로 화면 이동(팬), 5.4 공중 제스처 조작(선택 기능), 5.5 클릭 정밀도 향상 (자동), 5. 모드 1: PC 일반 작업 모드 (표시등: 파란색)

### Community 1107 - "6. 모드 2: 발표 모드 (표시등: 초록색)"
Cohesion: 0.33
Nodes (6): 6.1 진입 방법, 6.2 발표 화면 제어 버튼 매핑, 6.3 포인터 및 가상 레이저 조작, 6.4 발표 공중 제스처, 6.5 발표용 축 스냅 (자동), 6. 모드 2: 발표 모드 (표시등: 초록색)

### Community 1108 - "9. 기기 재연결 및 다중 장치(멀티 호스트) 관리"
Cohesion: 0.33
Nodes (6): 9.1 신규 장비 연결 절차, 9.2 연결 장치 전환 (최대 3대 등록, rev16 강화), 9.3 전환 시 안전 정책 (rev16 신규), 9.4 호스트 커스텀 라벨 관리 (rev16 신규), 9.5 호스트 전환 상태 확인, 9. 기기 재연결 및 다중 장치(멀티 호스트) 관리

### Community 1110 - "10. 충전과 자동 절전"
Cohesion: 0.33
Nodes (6): 10.1 충전, 10.2 자동 절전 1단계: 라이트 슬립 (가벼운 잠), 10.3 자동 절전 2단계: 딥 슬립 (깊은 잠), 10.4 안전 상태에서의 깨어남 제한, 10.5 절전에 안 들어가는 상황, 10. 충전과 자동 절전

### Community 1112 - "2. 아키텍처 결정 (4대 핵심 축)"
Cohesion: 0.40
Nodes (5): 2-1. 데이터 계층 구조 (3-Tier Layer), 2-2. Global + Override 슬롯 표현 (D3), 2-3. 매크로 자료구조 (B), 2-4. Action Kind 확장, 2. 아키텍처 결정 (4대 핵심 축)

### Community 1113 - "docs_v0415/Req_ImplPlan/30.impl_0410_001.md"
Cohesion: 0.40
Nodes (4): 파일 내용, 파일 변경, 파일 변경, 항목 기대 결과

### Community 1114 - "14. 매크로 관리 (자동 연속 동작)"
Cohesion: 0.33
Nodes (6): 14.1 매크로란?, 14.2 만들 수 있는 규모, 14.3 만드는 순서, 14.4 실생활 활용 예시, 14.5 안전장치, 14. 매크로 관리 (자동 연속 동작)

### Community 1115 - "3. 구성품 및 처음 사용하기"
Cohesion: 0.33
Nodes (6): 3.1 상자 안에 들어있는 것, 3.2 처음 전원 켤 때, 3.3 컴퓨터/스마트폰과 연결하기 (페어링), 3.4 두 번째, 세 번째 기기 등록하기, 3.5 다른 컴퓨터로 빠르게 전환하기, 3. 구성품 및 처음 사용하기

### Community 1116 - "5. 모드 1: PC 모드 (파란 불빛)"
Cohesion: 0.33
Nodes (6): 5.1 커서 움직이기, 5.2 기본 버튼 동작, 5.3 화면 스크롤과 좌우 이동, 5.4 공중 제스처 (손목 휙 돌리기), 5.5 자동으로 켜지는 정밀 기능, 5. 모드 1: PC 모드 (파란 불빛)

### Community 1117 - "6. 모드 2: 발표 모드 (초록 불빛)"
Cohesion: 0.33
Nodes (6): 6.1 모드 진입 방법, 6.2 발표 중 버튼 동작, 6.3 포인터와 레이저, 6.4 발표 중 제스처, 6.5 자동 수평/수직 정렬 (축 스냅), 6. 모드 2: 발표 모드 (초록 불빛)

### Community 1118 - "7. 모드 3: 스마트 TV 모드 (주황 불빛)"
Cohesion: 0.33
Nodes (6): 7.1 모드 진입 방법, 7.2 TV 조작 버튼, 7.3 TV 모드에서 안 되는 기능, 7.4 기울여 방향키 조작 (Tilt Hold), 7.5 커서가 필요한 앱에서, 7. 모드 3: 스마트 TV 모드 (주황 불빛)

### Community 1119 - "9. 여러 대의 컴퓨터/TV 관리하기"
Cohesion: 0.33
Nodes (6): 9.1 기기 등록 (최대 3대), 9.2 다른 기기로 전환하는 4가지 방법, 9.3 전환 중 안전장치, 9.4 기기에 별명 붙이기, 9.5 기기 상태 확인, 9. 여러 대의 컴퓨터/TV 관리하기

### Community 1123 - "3. 웹 UI 구성 옵션 (3단계)"
Cohesion: 0.50
Nodes (4): 3. 웹 UI 구성 옵션 (3단계), Option 1 — 확장형 슬롯 편집기 (최소 침습), Option 2 — 액션 중심 (Action-centric) 편집기, Option 3 — 매크로 지원 종합 편집기

### Community 1124 - "4. 트리거 자유도의 세부 옵션"
Cohesion: 0.50
Nodes (4): 4. 트리거 자유도의 세부 옵션, Tier A — 기존 트리거 재배치 (20개), Tier B — 트리거 파라미터 조정, Tier C — 새 트리거 정의

### Community 1125 - "7. 실시간 피드백 (UX 필수)"
Cohesion: 0.50
Nodes (4): 7. 실시간 피드백 (UX 필수), Live Test 버튼, Web Audio Preview, 트리거 시뮬레이션

### Community 1133 - "15. 검증 체크리스트 (P-J)"
Cohesion: 0.50
Nodes (4): 15. 검증 체크리스트 (P-J), Dead Code 제거, Status 확장, 회귀 테스트

### Community 1136 - "3. Config 스키마 (v5 통합 사양)"
Cohesion: 0.50
Nodes (4): 3-1. LittleFS 파일 구조, 3-2. Profile JSON 스키마 (`profile_N.json`), 3-3. 활성 프로파일 인덱스 JSON (`active_profile.json`), 3. Config 스키마 (v5 통합 사양)

### Community 1137 - "5. Global + Override 메커니즘"
Cohesion: 0.50
Nodes (4): 5-1. 저장 및 직렬화 최적화, 5-2. 런타임 슬롯 해석 ($O(1)$), 5-3. UI 시각적 피드백, 5. Global + Override 메커니즘

### Community 1138 - "5. 웹 REST API 사양 (v0412)"
Cohesion: 0.50
Nodes (4): 5-1. 프로파일 관리 엔드포인트, 5-2. 슬롯 및 트리거 테스트 엔드포인트, 5-3. 설정 및 호환성 엔드포인트, 5. 웹 REST API 사양 (v0412)

### Community 1139 - "6. 실행 흐름 (Runtime Flow)"
Cohesion: 0.50
Nodes (4): 6-1. 이벤트 디스패치 파이프라인, 6-2. commTask 매크로 시퀀서 및 안전 취소, 6-3. 프로파일 전환 시퀀스, 6. 실행 흐름 (Runtime Flow)

### Community 1140 - "7. Web UI 3-View 통합 설계"
Cohesion: 0.50
Nodes (4): 7-1. View 1: Slot-centric View (트리거 중심), 7-2. View 2: Action-centric View (기능 중심 역방향 매핑), 7-3. View 3: Macro Editor (매크로 시퀀스 편집기), 7. Web UI 3-View 통합 설계

### Community 1143 - "3. 기본 연결 및 작동법"
Cohesion: 0.40
Nodes (5): 3.1 전원 켜기, 3.2 기기 연결(블루투스 페어링) 및 등록 기기 전환, 3.3 기본 마우스 조작, 3.4 v0412 프로파일 자동 마이그레이션 (v0415 신규), 3. 기본 연결 및 작동법

### Community 1144 - "11. 웹 화면으로 상세 설정하기"
Cohesion: 0.40
Nodes (5): 11.1 웹 화면 접속 방법, 11.2 웹 화면 좌측 메뉴 (햄버거 ☰ 버튼), 11.3 프로필 관리 (드로어 상단), 11.4 상단 노란색 알림 띠, 11. 웹 화면으로 상세 설정하기

### Community 1148 - "ST_E10_ErrEvt_t"
Cohesion: 0.50
Nodes (4): ST_E10_ErrEvt_t, code, ts_ms, value

### Community 1149 - "ST_C10_ProfileSlots_t"
Cohesion: 0.17
Nodes (13): C10_ResolveSlot(), ST_C10_ProfileSlots_t, global, modes, overrideMask, C20_MakeMacro(), 15-1. P-A 선언부 사전 검증 체크리스트, 15-2. 다음 개발 단계 (Next Steps) (+5 more)

### Community 1156 - "12. 홈 탭 사용하기"
Cohesion: 0.50
Nodes (4): 12.1 필수 조작 카드, 12.2 기기 전환 카드, 12.3 가상 컨트롤러 (Virtual Remote), 12. 홈 탭 사용하기

### Community 1162 - "ST_Runtime_t"
Cohesion: 0.40
Nodes (5): ST_Runtime_t, active, lastRepeatMs, repeatMs, slot

### Community 1163 - "20. 사용자 지원"
Cohesion: 0.50
Nodes (4): 20. 사용자 지원, 기기 정보 확인 방법, 문의 시 포함할 정보, 문제가 생겼을 때

### Community 1164 - "8. 모드 전환과 상태 표시등"
Cohesion: 0.50
Nodes (4): 8.1 모드 순환 방법, 8.2 불빛(상태 표시등) 의미, 8.3 모드 전환 시 자동 정리, 8. 모드 전환과 상태 표시등

### Community 1167 - "am_offline_data_0415.js"
Cohesion: 0.40
Nodes (4): G_OFFLINE_DEFAULT_PROFILE_0, G_OFFLINE_KEYCODES, G_OFFLINE_START_TIME, G_OFFLINE_TRIGGERS

### Community 1169 - "6. 📐 명명 규칙 & 코드 정책"
Cohesion: 0.50
Nodes (4): 6.1. 명명 규칙 (Strict Naming Conventions), 6.2. ArduinoJson v7 전용 코딩 정책, 6.3. 안전 원자적 IO 정책 (Atomic Storage), 6. 📐 명명 규칙 & 코드 정책

### Community 1176 - "7.11. 전원 관리 시스템 (P20 & Light/Deep Sleep & WoM)"
Cohesion: 0.67
Nodes (3): 7.11.1. 설정 파라미터 (`ST_C10_PowerConfig_t`), 7.11.2. 슬립/웨이크 라이프사이클 및 안전 처리, 7.11. 전원 관리 시스템 (P20 & Light/Deep Sleep & WoM)

## Knowledge Gaps
- **1520 isolated node(s):** `name`, `version`, `description`, `keywords`, `authors` (+1515 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1803 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **36 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `CL_E10_EliteAirMouse` connect `CL_E10_EliteAirMouse` to `CL_B20_Ble`, `ST_C20_ActionSlot_t`, `E10_AirMouse_0415.h`, `CL_C10_Config`, `CL_C20_BtnDispatcher`, `CL_E10_EliteAirMouse::_sensorTask`, `W10_WebApi_Status_0415.cpp`, `ST_E10_DiagCounters`, `CL_L10_Led`, `6. 📐 명명 규칙 & 코드 정책`, `ST_C10_Macro_t`, `E10_AirMouse_Diag_0415.cpp`, `ST_C10_ProfileConfig_t`, `4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules)`, `E10_AirMouse_Hid_0415.cpp`, `CL_C20_ActionExec`, `CONTRACT_0415.md — 모듈 경계 및 인터페이스 계약`, `CL_E10_EliteAirMouse::begin`, `CL_M20_BiasTracker`, `E10_AirMouse_Task_0415.cpp`, `ST_MacroState_t`, `📝 AirMouse Elite S3 (v0410) 종합 가이드 & 기술 사양서`, `._pushFrame`, `CL_M10_AdvancedMotionProcessor`, `CL_M30_Gesture`, `E10_AirMouse_Action_0415.cpp`, `ST_E10_ErrEvt_t`, `E10_AirMouse_Core_0415.cpp`, `CL_P20_Power`?**
  _High betweenness centrality (0.266) - this node is a cross-community bridge._
- **Why does `CL_C10_Config` connect `CL_C10_Config` to `ST_C10_BootState_t`, `ST_C20_ActionSlot_t`, `C10_Config_Json_0415.cpp`, `ST_C10_MacroLib_t`, `2. 상태머신(FSM) 상세 명세`, `C10_Config_Valid_0415.cpp`, `C10_Def_0415.h`, `ST_C10_Macro_t`, `CL_C10_Config::patchProfileFromJson`, `CL_W10_WebConfig`, `ST_C10_ProfileConfig_t`, `4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules)`, `main.cpp`, `CL_E10_EliteAirMouse`, `CL_E10_EliteAirMouse::begin`?**
  _High betweenness centrality (0.105) - this node is a cross-community bridge._
- **Why does `cfgSave()` connect `10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11)` to `Missing UI Features / Gaps:`, `buildWakeMaskSafe`, `🎯 최종 권장 실행안`, `2. 상태머신(FSM) 상세 명세`, `ST_E10_DiagCounters`, `2. 함수 인터페이스 계약`, `0415_변경_001.md`, `Elite AirMouse S3 — 사용자 매뉴얼`?**
  _High betweenness centrality (0.084) - this node is a cross-community bridge._
- **Are the 4 inferred relationships involving `CL_E10_EliteAirMouse` (e.g. with `① 명명 규칙 (Strict Naming Conventions)` and `6.1. 명명 규칙 (Strict Naming Conventions)`) actually correct?**
  _`CL_E10_EliteAirMouse` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `LD2420` (e.g. with `Added` and `주요 특징`) actually correct?**
  _`LD2420` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `CL_C10_Config` (e.g. with `① 명명 규칙 (Strict Naming Conventions)` and `4. 개정 이력`) actually correct?**
  _`CL_C10_Config` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `ST_E10_Status_t` (e.g. with `① 명명 규칙 (Strict Naming Conventions)` and `명명 규칙 (엄격)`) actually correct?**
  _`ST_E10_Status_t` has 3 INFERRED edges - model-reasoned connections that need verification._