// =======================================================
// File: src/v0410/B20_Ble_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : B20_Ble_0410.h
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

class CL_B20_Ble {
  public:
    static constexpr uint8_t MAX_PEERS = 3;

    CL_B20_Ble();

    // 초기화 (E10.begin에서 호출)
    void begin();

    // ---- Bond 정보 ----
    uint8_t getBondCount() const;
    
    // ---- Active Peer Index (config 연동) ----
    uint8_t getActivePeerIndex() const { return _activePeerIndex; }
    void    setActivePeerIndex(uint8_t p_idx) { _activePeerIndex = (p_idx < MAX_PEERS) ? p_idx : 0; }

    // peer 순환 (index만 변경; 재연결은 Phase 9)
    uint8_t cycleActivePeer() {
        _activePeerIndex = (_activePeerIndex + 1) % MAX_PEERS;
        _dirty           = true;
        return _activePeerIndex;
    }

    // ====================================================
    // [Phase 9] 재연결 (disconnect + 재광고)
    //   NimBLE-Arduino 2.5.1 제약으로 whitelist 필터 없음.
    //   대상 peer 선택은 OS 자동재연결에 위임.
    // ====================================================
    bool reconnectToActivePeer(uint32_t p_whitelistMs = 10000);
    void clearWhitelist();

    // ---- Pairing Mode ----
    bool enterPairing(uint32_t p_timeoutMs = 30000);
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
    uint8_t _activePeerIndex = 0;
    
    // [M-1, R2-L-1] sensorTask(web/sensor 혼용 접근) — volatile로 재정렬/캐시 방지
    volatile bool     _pairing          = false;
    volatile uint32_t _pairingStartMs   = 0;
    volatile uint32_t _pairingTimeoutMs = 0;

    volatile bool _dirty = false;
  
    volatile bool     _whitelistActive  = false;
    volatile uint32_t _whitelistUntilMs = 0;

    // advertising 재시작 (라이브러리 소유 광고 위에 재구성)
    //   - NimBLE-Arduino 2.5.1 API 제약으로 whitelist 필터 없이 stop/start만
    bool _restartAdvertising();
};
