// =======================================================
// File: src/v0412/B20_Ble_0412.cpp
// =======================================================
#include "B20_Ble_0412.h"

#include <NimBLEDevice.h>
#include <vector>

#include "D10_Logger_0412.h"

// =======================================================
// ctor / begin
// =======================================================
CL_B20_Ble::CL_B20_Ble() {}

void CL_B20_Ble::begin() {
    _activePeerIndex = 0;
    _pairing         = false;
    _dirty           = false;

    D10_LOGI("[B20] begin: bond_count=%u", (unsigned)getBondCount());
}

// =======================================================
// Bond 조회 / 삭제
// =======================================================
uint8_t CL_B20_Ble::getBondCount() const {
    return (uint8_t)NimBLEDevice::getNumBonds();
}


// =======================================================
// Pairing Mode
// =======================================================
bool CL_B20_Ble::enterPairing(uint32_t p_timeoutMs) {
    if (_pairing) return true;

    _pairing          = true;
    _pairingStartMs   = (uint32_t)millis();
    _pairingTimeoutMs = p_timeoutMs;

    // 광고 재시작 (이미 광고 중이면 stop 후 start)
    NimBLEAdvertising* v_adv = NimBLEDevice::getAdvertising();
    if (v_adv) {
        v_adv->stop();
        v_adv->start();
    }

    D10_LOGI("[B20] enterPairing: timeout=%ums, bond_count=%u", (unsigned)p_timeoutMs, (unsigned)getBondCount());
    return true;
}

void CL_B20_Ble::exitPairing() {
    if (!_pairing) return;

    _pairing = false;

    // 광고는 계속 유지 (ESP32-BLE-CompositeHID가 관리)
    // 재광고는 라이브러리 책임

    D10_LOGI("[B20] exitPairing");
}

// =======================================================
// Tick (sensorTask)
// =======================================================
void CL_B20_Ble::tick(bool p_connected) {
    // ---- pairing mode ----
    if (_pairing) {
        if (p_connected) {
            D10_LOGI("[B20] pairing: connected, auto-exit");
            exitPairing();
        } else if ((uint32_t)millis() - _pairingStartMs >= _pairingTimeoutMs) {
            D10_LOGW("[B20] pairing: timeout");
            exitPairing();
        }
    }

    // ---- [Phase 9] whitelist 타임아웃 ----
    if (_whitelistActive) {
        // 연결 성공 시 즉시 해제
        if (p_connected) {
            D10_LOGI("[B20] whitelist: connected, clearing");
            clearWhitelist();
            return;
        }
        // 타임아웃
        if ((int32_t)((uint32_t)millis() - _whitelistUntilMs) >= 0) {
            D10_LOGW("[B20] whitelist: timeout, clearing");
            clearWhitelist();
        }
    }
}

// =======================================================
// [Phase 9] advertising 재시작 (disconnect-only 방식)
//   NimBLE-Arduino 2.5.1에서 advertising whitelist API가 노출되지 않음.
//   → whitelist 필터 없이 stop/start만. 대상 peer 선택은 OS 자동재연결에 위임.
// =======================================================
bool CL_B20_Ble::_restartAdvertising() {
    NimBLEAdvertising* v_adv = NimBLEDevice::getAdvertising();
    if (!v_adv) {
        D10_LOGW("[B20] no advertising instance");
        return false;
    }

    v_adv->stop();
    delay(30); // NimBLE 스택 반영 대기
    v_adv->start();

    D10_LOGI("[B20] advertising restarted");
    return true;
}

bool CL_B20_Ble::reconnectToActivePeer(uint32_t p_whitelistMs) {
    // 1) 현재 연결 모두 disconnect
    NimBLEServer* v_srv = NimBLEDevice::getServer();
    if (v_srv) {
        std::vector<uint16_t> v_peers = v_srv->getPeerDevices();
        for (uint16_t v_connId : v_peers) {
            D10_LOGI("[B20] disconnect connId=%u", (unsigned)v_connId);
            v_srv->disconnect(v_connId);
        }
        delay(80); // disconnect 반영 대기
    }

    // 2) 재광고 (whitelist 없음 — 모든 bond 허용)
    const bool v_ok = _restartAdvertising();

    if (v_ok) {
        _whitelistActive  = true; // "재연결 윈도우" 플래그
        _whitelistUntilMs = (uint32_t)millis() + p_whitelistMs;
        _dirty            = true;
    }

    return v_ok;
}

void CL_B20_Ble::clearWhitelist() {
    if (!_whitelistActive) return;
    _whitelistActive  = false;
    _whitelistUntilMs = 0;
    D10_LOGI("[B20] reconnect window cleared");
}
