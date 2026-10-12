# CLAUDE.md — Claude Code Project Guidelines

> **Target Project**: `esp32_AirMouse_002` (Elite AirMouse S3)  
> **Architecture**: ESP32-S3-Zero + MPU6050 BLE HID Composite AirMouse  
> **Single Source of Truth (SSOT)**: [agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)

---

## 📌 Single Source of Truth: agent.md 참조 안내

본 프로젝트의 모든 규정, 아키텍처 원칙, 하드웨어 핀맵, 코딩 및 명명 규칙, FreeRTOS 멀티태스킹 동기화 계약, 5대 계약 문서(CONTRACT, STATE, FLOW, BUDGET, SPEC) 기반 개발/리뷰 프로토콜, 필수 개발 6대 원칙, 표준 리뷰 응답 포맷(A~F), 빌드 및 검증 명령어 등은 **[agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)**에서 단일 통합 관리됩니다.

Claude Code는 분석, 설계, 구현, 리팩토링, 코드 리뷰, 검증 등 본 프로젝트의 모든 작업을 수행할 때 **반드시 [agent.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md)의 최신 지침을 최우선으로 참조하고 엄격히 준수**해야 합니다.

모든 신규 규칙 추가, 정책 수정 및 현행화 작업은 `agent.md`를 단일 기준으로 수정 및 반영합니다.
