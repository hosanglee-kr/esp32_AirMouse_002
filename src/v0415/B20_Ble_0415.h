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
 *  - Multi-Host Reject-Until-Target 지정 전환
 *
 * [v0415 Phase 2-3]
 *  - `_isReady()` inline 헬퍼 도입
 *    · NimBLEDevice::isInitialized() 직접 호출 금지 (SSOT)
 *    · 신규 API 추가 시 반드시 이 함수로 게이트
 *
 * [v0415 rev17]
 *  - B20_HOST 네임스페이스 상수화
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
#include <NimBLEDevice.h>    // ← Phase 2-3: _isReady() inline 위해 필수

namespace B20_BLE {
    static constexpr uint32_t PAIRING_TIMEOUT_MS   = 30000;
    static constexpr uint32_t ADV_RESTART_DELAY_MS = 30;
    static constexpr uint32_t DISCONN_SETTLE_MS    = 80;
}

// ======================================================
// [v0415 rev17] Multi-Host 상수 SSOT
// ======================================================
namespace B20_HOST {
    static constexpr uint32_t CONN_TIMEOUT_MS  = 10000;  // 타겟 호스트 연결 대기 (Strict, Option B)
    static constexpr uint8_t  BOND_MAX         = 3;      // 본딩 최대 개수 (C10_DEF::HOSTS_MAX 정합)
    static constexpr uint8_t  TARGET_IDX_NONE  = 0xFF;   // 타겟 미지정 센티넬
}

// [v0415 Multi-Host] 광고 상태 머신 enum
enum EN_B20_AdvMode_t : uint8_t {
    EN_B20_ADV_IDLE   = 0,
    EN_B20_ADV_CYCLE  = 1,
    EN_B20_ADV_TARGET = 2
};

// 전방 선언
class NimBLEAddress;
struct ble_gap_event;

bool isSameBleAddress(const NimBLEAddress& a, const NimBLEAddress& b);

// ======================================================
// BLE Manager 클래스
// ======================================================
class CL_B20_Ble {
  public:
    static constexpr uint8_t MAX_PEERS = B20_HOST::BOND_MAX;

    CL_B20_Ble();

    void begin();

    // ---- Bond 정보 ----
    uint8_t getBondCount() const;

    // ---- 연결된 peer 정보 조회 ----
    const char* getPeerName(char* p_buf, size_t p_bufSize) const;

    // ---- Active Peer Index ----
    uint8_t getActivePeerIndex() const { return _activePeerIndex; }
    void    setActivePeerIndex(uint8_t p_idx) {
        _activePeerIndex = (p_idx < MAX_PEERS) ? p_idx : 0;
    }

    uint8_t cycleActivePeer();

    // ---- Multi-Host Reject-Until-Target ----
    bool selectHostIdx(uint8_t p_idx,
                       uint32_t p_timeoutMs = B20_HOST::CONN_TIMEOUT_MS);
    bool selectHostAddr(const char* p_addr,
                        uint32_t p_timeoutMs = B20_HOST::CONN_TIMEOUT_MS);
    void cancelSelect();
    bool isSelecting() const { return _advMode == EN_B20_ADV_TARGET; }

    EN_B20_AdvMode_t getAdvMode() const { return _advMode; }
    uint8_t getTargetPeerIndex() const { return _targetPeerIdx; }

    // ---- 재연결 ----
    bool reconnectToActivePeer(uint32_t p_whitelistMs = 10000);
    void clearWhitelist();

    // ---- Pairing Mode ----
    bool enterPairing(uint32_t p_timeoutMs = B20_BLE::PAIRING_TIMEOUT_MS);
    void exitPairing();
    bool isPairing() const { return _pairing; }

    // ---- Dirty flag ----
    bool consumeDirty() {
        if (!_dirty) return false;
        _dirty = false;
        return true;
    }

    // ---- Tick (sensorTask) ----
    void tick(bool p_connected);

  private:
    // =====================================================
    // [v0415 Phase 2-3] NimBLE 초기화 상태 단일 접근점
    //   - NimBLEDevice::isInitialized() 직접 호출 금지
    //   - 신규 API 추가 시 반드시 이 함수로 게이트
    // =====================================================
    inline bool _isReady() const {
        return NimBLEDevice::isInitialized();
    }

    static int _gapEventHandler(struct ble_gap_event* p_event, void* p_arg);
    int  _onCustomGapEvent(struct ble_gap_event* p_event);
    void _handleGapConnect(uint16_t p_connHandle);
    void _ensureGapHandler();

    uint8_t _activePeerIndex = 0;

    // sensorTask(web/sensor 혼용 접근) — volatile
    volatile bool     _pairing          = false;
    volatile uint32_t _pairingStartMs   = 0;
    volatile uint32_t _pairingTimeoutMs = 0;

    volatile bool _dirty = false;

    // Multi-Host 광고 모드 및 타겟 인덱스
    volatile EN_B20_AdvMode_t _advMode          = EN_B20_ADV_IDLE;
    volatile uint8_t          _targetPeerIdx    = B20_HOST::TARGET_IDX_NONE;
    volatile uint32_t         _advModeStartMs   = 0;
    volatile uint32_t         _advModeTimeoutMs = B20_HOST::CONN_TIMEOUT_MS;

    volatile bool     _whitelistActive  = false;
    volatile uint32_t _whitelistUntilMs = 0;
    bool              _gapHandlerRegistered = false;

    bool _restartAdvertising();
};
