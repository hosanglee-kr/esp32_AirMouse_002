// =======================================================
// File: src/v040/B20_Ble_0400.cpp
// =======================================================
#include "B20_Ble_0400.h"

#include <NimBLEDevice.h>
#include <vector>

#include "D10_Logger_0400.h"

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

bool CL_B20_Ble::clearAllBonds() {
    const uint8_t v_n = getBondCount();
    if (v_n == 0) return true;

    NimBLEDevice::deleteAllBonds();
    D10_LOGW("[B20] deleteAllBonds: %u removed", (unsigned)v_n);
    return true;
}

uint8_t CL_B20_Ble::getConnectedCount() const {
    NimBLEServer* v_srv = NimBLEDevice::getServer();
    if (!v_srv) return 0;
    return (uint8_t)v_srv->getConnectedCount();
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

    D10_LOGI("[B20] enterPairing: timeout=%ums, bond_count=%u",
             (unsigned)p_timeoutMs, (unsigned)getBondCount());
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
// [Phase 9] whitelist 기반 재연결
// =======================================================
bool CL_B20_Ble::_restartAdvertisingWithWhitelist(bool p_useWhitelist) {
    NimBLEAdvertising* v_adv = NimBLEDevice::getAdvertising();
    if (!v_adv) {
        D10_LOGW("[B20] no advertising instance");
        return false;
    }

    // 1) 현재 광고 중지 (라이브러리 소유)
    v_adv->stop();
    delay(30);   // NimBLE 스택 반영 대기

    // 2) whitelist 정리 + 재구성
    v_adv->filterAcceptListClear();

    if (p_useWhitelist) {
        // 대상 peer address 얻기
        const int v_bondCount = NimBLEDevice::getNumBonds();
        if (v_bondCount == 0) {
            D10_LOGW("[B20] no bonds; skip whitelist");
            v_adv->setScanFilter(false);
            v_adv->start();
            return false;
        }

        // bond 목록 조회
        std::vector<NimBLEAddress> v_bonds = NimBLEDevice::getBondedDevices();
        if (v_bonds.empty()) {
            D10_LOGW("[B20] bonded devices empty");
            v_adv->setScanFilter(false);
            v_adv->start();
            return false;
        }

        // active peer index가 범위 초과면 0으로
        if (_activePeerIndex >= (uint8_t)v_bonds.size()) {
            _activePeerIndex = 0;
        }

        const NimBLEAddress& v_target = v_bonds[_activePeerIndex];
        v_adv->filterAcceptListAdd(v_target);
        v_adv->setScanFilter(true);   // accept list(whitelist) only

        D10_LOGI("[B20] advertising with whitelist: peer[%u]=%s",
                 (unsigned)_activePeerIndex, v_target.toString().c_str());
    } else {
        v_adv->setScanFilter(false);
        D10_LOGI("[B20] advertising: no whitelist (accept all)");
    }

    // 3) 재시작
    v_adv->start();
    return true;
}

bool CL_B20_Ble::reconnectToActivePeer(uint32_t p_whitelistMs) {
    // 1) 현재 연결 모두 disconnect (라이브러리 서버 소유)
    NimBLEServer* v_srv = NimBLEDevice::getServer();
    if (v_srv) {
        const auto v_peers = v_srv->getPeerDevices();
        for (const auto& p : v_peers) {
            D10_LOGI("[B20] disconnect connId=%u", (unsigned)p.first);
            v_srv->disconnect(p.first);
        }
        delay(80);   // disconnect 반영 대기
    }

    // 2) whitelist 필터로 광고 재시작
    const bool v_ok = _restartAdvertisingWithWhitelist(true);

    if (v_ok) {
        _whitelistActive  = true;
        _whitelistUntilMs = (uint32_t)millis() + p_whitelistMs;
        _dirty            = true;
    }

    return v_ok;
}

void CL_B20_Ble::clearWhitelist() {
    if (!_whitelistActive) return;
    _whitelistActive = false;
    _whitelistUntilMs = 0;
    (void)_restartAdvertisingWithWhitelist(false);
    D10_LOGI("[B20] whitelist cleared (accept all)");
}
