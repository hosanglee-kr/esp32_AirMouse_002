// =======================================================
// File: src/v0415/B20_Ble_0415.cpp
// =======================================================
#include "B20_Ble_0415.h"

#include <NimBLEDevice.h>
#include <vector>

#include "D10_Logger_0415.h"

// =======================================================
// ctor / begin
// =======================================================
CL_B20_Ble::CL_B20_Ble() {}

void CL_B20_Ble::begin() {
    _activePeerIndex = 0;
    _pairing         = false;
    _dirty           = false;

    // taskServer가 비동기로 NimBLEDevice::init()을 완료할 때까지 대기 (최대 500ms)
    const uint32_t v_t0 = (uint32_t)millis();
    while (!NimBLEDevice::isInitialized() && ((uint32_t)millis() - v_t0 < 500)) {
        delay(10);
    }

    D10_LOGI("[B20] begin: bond_count=%u", (unsigned)getBondCount());
}

// =======================================================
// Bond 조회 / 삭제
// =======================================================
uint8_t CL_B20_Ble::getBondCount() const {
    if (!NimBLEDevice::isInitialized()) {
        return 0;
    }
    return (uint8_t)NimBLEDevice::getNumBonds();
}

// =======================================================
// [v0415 Wave F] 연결된 peer 이름 조회
// -------------------------------------------------------
// NimBLE-Arduino 2.5.x API 사용:
//   - getConnectedCount() > 0 확인
//   - getPeerDevices() → 연결된 connId 벡터
//   - getPeerInfo(connId) → NimBLEConnInfo
//   - getName() (없으면 getAddress().toString())
// =======================================================
const char* CL_B20_Ble::getPeerName(char* p_buf, size_t p_bufSize) const {
    if (!p_buf || p_bufSize < 4) return nullptr;
    p_buf[0] = '\0';

    if (!NimBLEDevice::isInitialized()) return nullptr;

    NimBLEServer* v_srv = NimBLEDevice::getServer();
    if (!v_srv) return nullptr;

    if (v_srv->getConnectedCount() == 0) return nullptr;

    std::vector<uint16_t> v_peers = v_srv->getPeerDevices();
    if (v_peers.empty()) return nullptr;

    // 첫 연결 peer의 정보 조회
    NimBLEConnInfo v_info = v_srv->getPeerInfo(v_peers[0]);
    std::string v_name = v_info.getName();
    if (!v_name.empty()) {
        snprintf(p_buf, p_bufSize, "%s", v_name.c_str());
    } else {
        snprintf(p_buf, p_bufSize, "%s", v_info.getAddress().toString().c_str());
    }
    return p_buf;
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
    delay(B20_BLE::ADV_RESTART_DELAY_MS); // NimBLE 스택 반영 대기
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
        delay(B20_BLE::DISCONN_SETTLE_MS); // disconnect 반영 대기
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
