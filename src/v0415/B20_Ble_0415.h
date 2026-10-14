// =======================================================
// File: src/v0415/B20_Ble_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : B20_Ble_0415.h
 * 모듈약어 : B20
 * 모듈명 : BLE Manager (Pairing / Bonds)
 * ------------------------------------------------------
 * 기능 요약
 *  - Bond 관리 (조회 / 삭제)
 *  - Pairing Mode (30s 타임아웃, 연결 시 자동 종료)
 *  - Active Peer Index 추적 (config 연동)
 *
 * [설계]
 *  - NimBLE-Arduino 2.5.x API 사용
 *  - 실제 advertising은 ESP32-BLE-CompositeHID가 소유. B20은 조회/제어만.
 *  - 재연결 로직은 Phase 9에서 (Host Cycle) 별도 추가.
 *
 * [v0415 Wave F 주요 변경]
 *  - getPeerName() 신규: 연결된 peer의 이름/MAC 조회 (관측용)
 *    · NimBLE 2.5.x: getConnectedCount + getPeerDevices + getPeerInfo
 *    · 홈 상단 pill에 peer 이름 노출용
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명 : CL_모듈약어_ 접두사
 *   - private  : _ 접두사
 *   - 로컬     : v_ 접두사
 *   - 인자     : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

namespace B20_BLE {
    static constexpr uint32_t PAIRING_TIMEOUT_MS  = 30000;
    static constexpr uint32_t ADV_RESTART_DELAY_MS = 30;
    static constexpr uint32_t DISCONN_SETTLE_MS    = 80;
}

// ======================================================
// [v0415 rev17] Multi-Host 상수 SSOT
// ------------------------------------------------------
//  - SPEC_0415.md §부록 및 CONTRACT §2.21과 1:1 정합
//  - 하드코딩 리터럴(10000, 0xFF, 3)을 명명 상수로 통합
//  - 상위 C10_DEF::HOSTS_MAX(=3)와 반드시 동일 값 유지
// ======================================================
namespace B20_HOST {
    static constexpr uint32_t CONN_TIMEOUT_MS  = 10000;  // 타겟 호스트 연결 대기 (Strict, Option B)
    static constexpr uint8_t  BOND_MAX         = 3;      // 본딩 최대 개수 (C10_DEF::HOSTS_MAX 정합)
    static constexpr uint8_t  TARGET_IDX_NONE  = 0xFF;   // 타겟 미지정 센티넬
}

// [v0415 Multi-Host] 광고 상태 머신 enum
enum EN_B20_AdvMode_t : uint8_t {
    EN_B20_ADV_IDLE   = 0,  // 일반 대기/연결 상태
    EN_B20_ADV_CYCLE  = 1,  // 단순 순환 모드 (구 whitelist)
    EN_B20_ADV_TARGET = 2   // 특정 호스트 지정 전환 (Reject-Until-Target)
};

// 전방 선언
class NimBLEAddress;
struct ble_gap_event;

// 순수 6바이트 MAC 동일성 비교 헬퍼 (NimBLEAddress::equals()의 type 불일치 False Negative 방지)
bool isSameBleAddress(const NimBLEAddress& a, const NimBLEAddress& b);

class CL_B20_Ble {
  public:
    // [v0415 rev17] B20_HOST::BOND_MAX와 alias (기존 심볼 호환)
    static constexpr uint8_t MAX_PEERS = B20_HOST::BOND_MAX;

    CL_B20_Ble();

    // 초기화 (E10.begin에서 호출)
    void begin();

    // ---- Bond 정보 ----
    uint8_t getBondCount() const;

    // ====================================================
    // [v0415 Wave F] 연결된 peer 정보 조회 (관측용)
    // ----------------------------------------------------
    //  - NimBLE 2.5.x API: getPeerDevices() + getPeerInfo()
    //  - 반환: p_buf에 peer 이름 (없으면 MAC 주소)
    //  - 미연결 시 nullptr 반환 + p_buf[0]='\0'
    // ====================================================
    const char* getPeerName(char* p_buf, size_t p_bufSize) const;

    // ---- Active Peer Index (config 연동) ----
    uint8_t getActivePeerIndex() const { return _activePeerIndex; }
    void    setActivePeerIndex(uint8_t p_idx) { _activePeerIndex = (p_idx < MAX_PEERS) ? p_idx : 0; }

    // peer 순환 (기존 cycleActivePeer를 강제 순환 지정 전환으로 업그레이드)
    uint8_t cycleActivePeer();

    // ====================================================
    // [v0415 Multi-Host Phase 2] Reject-Until-Target 지정 전환
    // ====================================================
    bool selectHostIdx(uint8_t p_idx,
                       uint32_t p_timeoutMs = B20_HOST::CONN_TIMEOUT_MS);
    bool selectHostAddr(const char* p_addr,
                        uint32_t p_timeoutMs = B20_HOST::CONN_TIMEOUT_MS);
    void cancelSelect();
    bool isSelecting() const { return _advMode == EN_B20_ADV_TARGET; }

    EN_B20_AdvMode_t getAdvMode() const { return _advMode; }
    uint8_t getTargetPeerIndex() const { return _targetPeerIdx; }

    // ====================================================
    // [Phase 9] 재연결 (disconnect + 재광고)
    //   NimBLE-Arduino 2.5.1 제약으로 whitelist 필터 없음.
    //   대상 peer 선택은 OS 자동재연결에 위임.
    // ====================================================
    bool reconnectToActivePeer(uint32_t p_whitelistMs = 10000);
    void clearWhitelist();

    // ---- Pairing Mode ----
    bool enterPairing(uint32_t p_timeoutMs = B20_BLE::PAIRING_TIMEOUT_MS);
    void exitPairing();
    bool isPairing() const { return _pairing; }

    // ---- Dirty flag (config 저장 필요 여부) ----
    bool consumeDirty() {
        if (!_dirty) return false;
        _dirty = false;
        return true;
    }

    // ---- Tick (sensorTask에서 호출) ----
    void tick(bool p_connected);

  private:
    static int _gapEventHandler(struct ble_gap_event* p_event, void* p_arg);
    int  _onCustomGapEvent(struct ble_gap_event* p_event);
    void _handleGapConnect(uint16_t p_connHandle);
    void _ensureGapHandler();

    uint8_t _activePeerIndex = 0;

    // [M-1, R2-L-1] sensorTask(web/sensor 혼용 접근) — volatile로 재정렬/캐시 방지
    volatile bool     _pairing          = false;
    volatile uint32_t _pairingStartMs   = 0;
    volatile uint32_t _pairingTimeoutMs = 0;

    volatile bool _dirty = false;

    // [v0415 Multi-Host] 광고 모드 및 타겟 인덱스 통합 관리
    volatile EN_B20_AdvMode_t _advMode          = EN_B20_ADV_IDLE;
    volatile uint8_t          _targetPeerIdx    = B20_HOST::TARGET_IDX_NONE;
    volatile uint32_t         _advModeStartMs   = 0;
    volatile uint32_t         _advModeTimeoutMs = B20_HOST::CONN_TIMEOUT_MS;

    volatile bool     _whitelistActive  = false;
    volatile uint32_t _whitelistUntilMs = 0;
    bool              _gapHandlerRegistered = false;

    // advertising 재시작 (라이브러리 소유 광고 위에 재구성)
    bool _restartAdvertising();
};

