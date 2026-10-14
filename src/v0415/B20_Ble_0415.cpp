// =======================================================
// File: src/v0415/B20_Ble_0415.cpp
// =======================================================
#include "B20_Ble_0415.h"

#include <NimBLEDevice.h>
#include <vector>

#include "D10_Logger_0415.h"

// =======================================================
// [v0415 Multi-Host] 6바이트 MAC 동일성 비교 헬퍼
// =======================================================
bool isSameBleAddress(const NimBLEAddress& a, const NimBLEAddress& b) {
    return memcmp(a.getVal(), b.getVal(), 6) == 0;
}

// =======================================================
// ctor / begin
// =======================================================
CL_B20_Ble::CL_B20_Ble() {
    _advMode = EN_B20_ADV_IDLE;
    _targetPeerIdx = B20_HOST::TARGET_IDX_NONE;
}

void CL_B20_Ble::begin() {
    _activePeerIndex = 0;
    _pairing         = false;
    _dirty           = false;
    _advMode         = EN_B20_ADV_IDLE;
    _targetPeerIdx   = B20_HOST::TARGET_IDX_NONE;

    // taskServer가 비동기로 NimBLEDevice::init()을 완료할 때까지 대기 (최대 500ms)
    const uint32_t v_t0 = (uint32_t)millis();
    while (!NimBLEDevice::isInitialized() && ((uint32_t)millis() - v_t0 < 500)) {
        delay(10);
    }

    // [v0415 Multi-Host] RPA (호스트 기반 프라이버시) 활성화 (iOS/macOS RPA 호환)
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RPA_RANDOM_DEFAULT);

    _ensureGapHandler();

    D10_LOGI("[B20] begin: bond_count=%u, rpa_enabled", (unsigned)getBondCount());
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
// [v0415 Wave F] 연결된 peer 정보 조회
// -------------------------------------------------------
// NimBLE-Arduino 2.5.1 API 한계:
//   - NimBLEConnInfo는 getName() 미제공 (client API 전용)
//   - Server-side peer의 Device Name은 GATT read가 필요
//   - 관측 API 단순성 유지를 위해 MAC 주소만 노출
//   - getPeerInfo(connId).getAddress().toString() → "AA:BB:CC:DD:EE:FF"
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

    // NimBLE 2.5.1: NimBLEConnInfo는 getAddress() 제공
    //   - getName()은 client API 전용이라 server-side peer 이름 접근 불가
    //   - MAC 주소를 노출하여 연결 peer 식별
    NimBLEConnInfo v_info = v_srv->getPeerInfo(v_peers[0]);
    snprintf(p_buf, p_bufSize, "%s", v_info.getAddress().toString().c_str());
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
// [v0415 Multi-Host Phase 2] GAP Event Handler & Reject Engine
// =======================================================
void CL_B20_Ble::_ensureGapHandler() {
    if (_gapHandlerRegistered) return;
    if (NimBLEDevice::isInitialized()) {
        if (NimBLEDevice::setCustomGapHandler(_gapEventHandler, this)) {
            _gapHandlerRegistered = true;
            D10_LOGI("[B20] custom GAP event handler registered");
        }
    }
}

int CL_B20_Ble::_gapEventHandler(struct ble_gap_event* p_event, void* p_arg) {
    CL_B20_Ble* v_this = static_cast<CL_B20_Ble*>(p_arg);
    if (v_this) {
        return v_this->_onCustomGapEvent(p_event);
    }
    return 0;
}

int CL_B20_Ble::_onCustomGapEvent(struct ble_gap_event* p_event) {
    if (!p_event) return 0;
    if (p_event->type == BLE_GAP_EVENT_CONNECT) {
        if (p_event->connect.status == 0) {
            _handleGapConnect(p_event->connect.conn_handle);
        }
    }
    return 0;
}

void CL_B20_Ble::_handleGapConnect(uint16_t p_connHandle) {
    if (_advMode != EN_B20_ADV_TARGET) {
        _advMode = EN_B20_ADV_IDLE;
        return;
    }

    NimBLEServer* v_srv = NimBLEDevice::getServer();
    if (!v_srv) return;

    NimBLEConnInfo v_info = v_srv->getPeerInfoByHandle(p_connHandle);
    NimBLEAddress v_connIdAddr = v_info.getIdAddress(); // Identity Address

    const int v_bonds = (int)NimBLEDevice::getNumBonds();
    if (_targetPeerIdx < v_bonds) {
        NimBLEAddress v_targetAddr = NimBLEDevice::getBondedAddress(_targetPeerIdx);
        if (!isSameBleAddress(v_connIdAddr, v_targetAddr)) {
            // 비대상 기기: 즉시 0x16 (Connection Terminated By Local Host) 거절
            D10_LOGW("[B20] Reject non-target peer (connHandle=%u, addr=%s)",
                     (unsigned)p_connHandle, v_connIdAddr.toString().c_str());
            v_srv->disconnect(p_connHandle, 0x16);
            return;
        }
    }

    // 목표 기기 접속 성공!
    D10_LOGI("[B20] Target host connected! idx=%u, addr=%s",
             (unsigned)_targetPeerIdx, v_connIdAddr.toString().c_str());
    _advMode = EN_B20_ADV_IDLE;
    _activePeerIndex = _targetPeerIdx;
    _targetPeerIdx = B20_HOST::TARGET_IDX_NONE;
    _dirty = true;
}

// =======================================================
// [v0415 Multi-Host Phase 2] selectHostIdx & cycleActivePeer
// =======================================================
bool CL_B20_Ble::selectHostIdx(uint8_t p_idx, uint32_t p_timeoutMs) {
    const int v_bonds = (int)getBondCount();
    if (v_bonds <= 0 || p_idx >= v_bonds) {
        D10_LOGW("[B20] selectHostIdx failed: invalid idx=%u (bonds=%d)", (unsigned)p_idx, v_bonds);
        return false;
    }

    _ensureGapHandler();

    NimBLEServer* v_srv = NimBLEDevice::getServer();
    // 이미 대상 기기에 연결되어 있으면 no-op
    if (v_srv && v_srv->getConnectedCount() > 0) {
        std::vector<uint16_t> v_peers = v_srv->getPeerDevices();
        if (!v_peers.empty()) {
            NimBLEAddress v_currAddr = v_srv->getPeerInfo(v_peers[0]).getIdAddress();
            NimBLEAddress v_targetAddr = NimBLEDevice::getBondedAddress(p_idx);
    
            if (isSameBleAddress(v_currAddr, v_targetAddr)) {
                D10_LOGI("[B20] already connected to target host idx=%u", (unsigned)p_idx);
                _activePeerIndex = p_idx;
                _targetPeerIdx   = B20_HOST::TARGET_IDX_NONE;
                _advMode         = EN_B20_ADV_IDLE;
                return true;
            }
        }
    }


    // 1) 기존 연결 모두 강제 해제 (0x16)
    if (v_srv && v_srv->getConnectedCount() > 0) {
        std::vector<uint16_t> v_peers = v_srv->getPeerDevices();
        for (uint16_t v_h : v_peers) {
            v_srv->disconnect(v_h, 0x16);
        }
        delay(B20_BLE::DISCONN_SETTLE_MS);
    }

    // 2) Target 모드 설정 및 재광고
    _targetPeerIdx    = p_idx;
    _advModeStartMs   = (uint32_t)millis();
    _advModeTimeoutMs = (p_timeoutMs > 0) ? p_timeoutMs : B20_HOST::CONN_TIMEOUT_MS;
    _advMode          = EN_B20_ADV_TARGET;
    _whitelistActive  = false;

    const bool v_ok = _restartAdvertising();
    D10_LOGI("[B20] selectHostIdx(%u): started (timeout=%ums)", (unsigned)p_idx, (unsigned)_advModeTimeoutMs);
    return v_ok;
}

bool CL_B20_Ble::selectHostAddr(const char* p_addr, uint32_t p_timeoutMs) {
    if (!p_addr || strlen(p_addr) < 17) return false;
    const int v_bonds = (int)getBondCount();
    for (int i = 0; i < v_bonds; i++) {
        NimBLEAddress v_b = NimBLEDevice::getBondedAddress(i);
        if (strcasecmp(v_b.toString().c_str(), p_addr) == 0) {
            return selectHostIdx((uint8_t)i, p_timeoutMs);
        }
    }
    D10_LOGW("[B20] selectHostAddr: not in bond list '%s'", p_addr);
    return false;
}

void CL_B20_Ble::cancelSelect() {
    if (_advMode != EN_B20_ADV_TARGET) return;
    _advMode       = EN_B20_ADV_IDLE;
    _targetPeerIdx = B20_HOST::TARGET_IDX_NONE;
    D10_LOGI("[B20] host select cancelled -> IDLE");
}

uint8_t CL_B20_Ble::cycleActivePeer() {
    const uint8_t v_bonds = getBondCount();
    if (v_bonds > 0) {
        _activePeerIndex = (_activePeerIndex + 1) % v_bonds;
    } else {
        _activePeerIndex = 0;
    }
    
    _dirty = true;
    D10_LOGI("[B20] cycleActivePeer: next idx=%u", (unsigned)_activePeerIndex);
    // [의사결정 2] 순환 시 다음 타깃으로 즉시 강제 전환 시작!
    (void)selectHostIdx(_activePeerIndex, B20_HOST::CONN_TIMEOUT_MS);
    return _activePeerIndex;
}

// =======================================================
// Tick (sensorTask)
// =======================================================
void CL_B20_Ble::tick(bool p_connected) {
    _ensureGapHandler();

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

    // ---- [v0415 Multi-Host] Target 모드 타임아웃 검사 ----
    if (_advMode == EN_B20_ADV_TARGET) {
        if (p_connected && _targetPeerIdx == B20_HOST::TARGET_IDX_NONE) {
            _advMode = EN_B20_ADV_IDLE;
            return;
        }
    
        if ((uint32_t)millis() - _advModeStartMs >= _advModeTimeoutMs) {
            // [의사결정 1: Strict] 타임아웃 시 연결 시도 즉시 중단 (대기 IDLE 복귀)
            D10_LOGW("[B20] selectHost timeout (%ums) -> Strict Stop (IDLE)", (unsigned)_advModeTimeoutMs);
            cancelSelect();
        }
    }

    // ---- [Phase 9] legacy whitelist 타임아웃 ----
    if (_whitelistActive) {
        if (p_connected) {
            D10_LOGI("[B20] whitelist: connected, clearing");
            clearWhitelist();
            return;
        }
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
