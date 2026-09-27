// =======================================================
// File: src/v040/B20_Ble_0400.cpp
// =======================================================
#include "B20_Ble_0400.h"

#include <NimBLEDevice.h>

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
    if (!_pairing) return;

    // 연결 발생 시 자동 종료
    if (p_connected) {
        D10_LOGI("[B20] pairing: connected, auto-exit");
        exitPairing();
        return;
    }

    // 타임아웃
    if ((uint32_t)millis() - _pairingStartMs >= _pairingTimeoutMs) {
        D10_LOGW("[B20] pairing: timeout");
        exitPairing();
    }
}
