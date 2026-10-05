# Changelog

모든 주요 변경 사항은 이 파일에 기록됩니다.
[Semantic Versioning](https://semver.org/)을 따릅니다.

## [0.0.1] - 2026-10-06

### Added
- 초기 릴리스
- `LD2420` 메인 클래스 (FreeRTOS 기반, 논블로킹)
- `LD2420Protocol` 프레임 빌더/파서
- `LD2420Calibration` 자동 캘리브레이션 관리자
- 16게이트 개별 임계값 설정
- 자동 캘리브레이션 (노이즈 플로어 × 5/× 3)
- 프리셋: default / sensitive / balanced
- 비동기 명령 큐 + ACK 재시도 (최대 3회, 1초 타임아웃)
- 이벤트/데이터 콜백
- 예제 10종

### Compatibility
- ESP32 (모든 변형), ESP32-S2/S3/C3/C6
- LD2420 펌웨어 v1.5.3 이상
- Arduino ESP32 Core 2.x / 3.x
- PlatformIO espressif32 >= 5.0.0

### Known Issues
- AVR 보드 미지원 (FreeRTOS 의존)
- 펌웨어 v1.5.2 이하 보레이트(256000) 미검증
