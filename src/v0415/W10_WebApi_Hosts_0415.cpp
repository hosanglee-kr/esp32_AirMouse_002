/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Hosts_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Multi-Host API
 * ------------------------------------------------------
 * [v0415 Phase 2-1]
 *  - apiHostsSelect / apiHostsLabel: _parseBodyAndGate 사용
 *  - apiHostsCancel: Body 없음 (게이트만)
 *
 * [v0415 Phase 2-5]
 *  - _findHostByAddr() 헬퍼 (익명 namespace)
 *    · apiHostsGet / apiHostsLabel의 MAC 순회 통합
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"
#include <NimBLEDevice.h>

namespace {

// =====================================================
// [v0415 Phase 2-5] C10 호스트 목록에서 MAC 주소로 인덱스 검색
//   - 대소문자 무시 (strcasecmp)
//   - 미발견 시 -1 반환
// =====================================================
int8_t _findHostByAddr(const ST_C10_HostsConfig_t& p_cfg, const char* p_addr) {
    if (!p_addr || p_addr[0] == '\0') return -1;
    for (uint8_t i = 0; i < p_cfg.count; i++) {
        if (strcasecmp(p_cfg.hosts[i].addr, p_addr) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

}  // namespace

// =====================================================
// GET /api/hosts
// =====================================================
void CL_W10_WebConfig::apiHostsGet(AsyncWebServerRequest* req) {
    if (_gateSafeModeOrReply(req)) return;

    ST_W10_E10If_t* v_e10if = _e10if;
    if (!v_e10if || !v_e10if->getHostState) {
        _sendErr(req, "not_ready", "E10 host interface not available");
        return;
    }

    uint8_t v_bondCount = 0;
    uint8_t v_activeIdx = 0;
    uint8_t v_targetIdx = 0xFF;
    bool    v_selecting = false;
    char    v_activePeerName[32] = "";

    v_e10if->getHostState(v_e10if->ctx,
                          &v_bondCount,
                          &v_activeIdx,
                          &v_targetIdx,
                          &v_selecting,
                          v_activePeerName,
                          sizeof(v_activePeerName));

    ST_C10_HostsConfig_t v_hostsCfg;
    memset(&v_hostsCfg, 0, sizeof(v_hostsCfg));
    if (_cfg) {
        _cfg->loadHosts(v_hostsCfg);
    }

    JsonDocument v_doc;
    v_doc["code"] = "ok";
    v_doc["msg"]  = "ok";

    JsonObject v_data = v_doc["data"].to<JsonObject>();
    v_data["bond_count"]       = v_bondCount;
    v_data["active_idx"]       = v_activeIdx;
    v_data["target_idx"]       = v_targetIdx;
    v_data["is_selecting"]     = v_selecting;
    v_data["active_peer_name"] = v_activePeerName;

    JsonArray v_arr = v_data["hosts"].to<JsonArray>();

    for (uint8_t i = 0; i < v_bondCount && i < C10_DEF::HOSTS_MAX; i++) {
        NimBLEAddress v_bAddr = NimBLEDevice::getBondedAddress(i);
        String v_strAddr = v_bAddr.toString().c_str();

        JsonObject v_item = v_arr.add<JsonObject>();
        v_item["idx"]       = i;
        v_item["addr"]      = v_strAddr;
        v_item["is_active"] = (i == v_activeIdx && strlen(v_activePeerName) > 0);
        v_item["is_target"] = (v_selecting && (i == v_targetIdx));

        // C10 설정 매칭 (라벨, 색상)
        const char* v_label = "";
        const char* v_color = "#3B82F6";
        int8_t v_h = _findHostByAddr(v_hostsCfg, v_strAddr.c_str());
        if (v_h >= 0) {
            v_label = v_hostsCfg.hosts[v_h].label;
            v_color = v_hostsCfg.hosts[v_h].color;
        }
        v_item["label"] = v_label;
        v_item["color"] = v_color;
    }

    _sendJsonStream(req, v_doc, 200);
}

// =====================================================
// POST /api/hosts/select
// =====================================================
void CL_W10_WebConfig::apiHostsSelect(AsyncWebServerRequest* req,
                                      uint8_t* data, size_t len,
                                      size_t index, size_t total) {
    JsonDocument v_doc;
    if (!_parseBodyAndGate(req, data, len, index, total, v_doc)) return;

    if (v_doc["idx"].isNull()) {
        _sendErr(req, "bad_param", "Missing 'idx' field");
        return;
    }

    uint8_t v_idx = v_doc["idx"].as<uint8_t>();

    if (v_idx >= C10_DEF::HOSTS_MAX) {
        _sendErr(req, "out_of_range", "Host index out of range");
        return;
    }

    ST_W10_E10If_t* v_e10if = _e10if;
    if (!v_e10if || !v_e10if->requestHostOp) {
        _sendErr(req, "not_ready", "E10 interface unavailable");
        return;
    }

    EN_C20_ReqHostOp_t v_op = EN_C20_REQ_HOST_NONE;
    if (v_idx == 0)      v_op = EN_C20_REQ_HOST_SEL_0;
    else if (v_idx == 1) v_op = EN_C20_REQ_HOST_SEL_1;
    else if (v_idx == 2) v_op = EN_C20_REQ_HOST_SEL_2;

    v_e10if->requestHostOp(v_e10if->ctx, (uint8_t)v_op);

    _sendOk(req, "ok", "Host select requested");
}

// =====================================================
// POST /api/hosts/cancel (Body 없음 - 게이트만)
// =====================================================
void CL_W10_WebConfig::apiHostsCancel(AsyncWebServerRequest* req,
                                      uint8_t* data, size_t len,
                                      size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    (void)data; (void)len; (void)index; (void)total;

    ST_W10_E10If_t* v_e10if = _e10if;
    if (!v_e10if || !v_e10if->requestHostOp) {
        _sendErr(req, "not_ready", "E10 interface unavailable");
        return;
    }

    v_e10if->requestHostOp(v_e10if->ctx, (uint8_t)EN_C20_REQ_HOST_CANCEL);

    _sendOk(req, "ok", "Host select cancelled");
}

// =====================================================
// POST /api/hosts/label
// =====================================================
void CL_W10_WebConfig::apiHostsLabel(AsyncWebServerRequest* req,
                                     uint8_t* data, size_t len,
                                     size_t index, size_t total) {
    JsonDocument v_doc;
    if (!_parseBodyAndGate(req, data, len, index, total, v_doc)) return;

    const char* v_addr  = v_doc["addr"].as<const char*>();
    const char* v_label = v_doc["label"].as<const char*>();
    const char* v_color = v_doc["color"].as<const char*>();

    if (!v_addr || v_addr[0] == '\0') {
        _sendErr(req, "bad_param", "Missing 'addr' field");
        return;
    }

    if (!_cfg) {
        _sendErr(req, "no_config", "Config module unavailable");
        return;
    }

    ST_C10_HostsConfig_t v_hostsCfg;
    memset(&v_hostsCfg, 0, sizeof(v_hostsCfg));
    _cfg->loadHosts(v_hostsCfg);

    // [Phase 2-5] 통합 헬퍼 사용
    int8_t v_matchIdx = _findHostByAddr(v_hostsCfg, v_addr);

    if (v_matchIdx >= 0) {
        if (v_label) strlcpy(v_hostsCfg.hosts[v_matchIdx].label, v_label,
                             sizeof(v_hostsCfg.hosts[v_matchIdx].label));
        if (v_color) strlcpy(v_hostsCfg.hosts[v_matchIdx].color, v_color,
                             sizeof(v_hostsCfg.hosts[v_matchIdx].color));
    } else {
        if (v_hostsCfg.count < C10_DEF::HOSTS_MAX) {
            uint8_t v_newIdx = v_hostsCfg.count++;
            strlcpy(v_hostsCfg.hosts[v_newIdx].addr, v_addr,
                    sizeof(v_hostsCfg.hosts[v_newIdx].addr));
            if (v_label) strlcpy(v_hostsCfg.hosts[v_newIdx].label, v_label,
                                 sizeof(v_hostsCfg.hosts[v_newIdx].label));
            else         strlcpy(v_hostsCfg.hosts[v_newIdx].label, "Host",
                                 sizeof(v_hostsCfg.hosts[v_newIdx].label));
            if (v_color) strlcpy(v_hostsCfg.hosts[v_newIdx].color, v_color,
                                 sizeof(v_hostsCfg.hosts[v_newIdx].color));
            else         strlcpy(v_hostsCfg.hosts[v_newIdx].color, "#3B82F6",
                                 sizeof(v_hostsCfg.hosts[v_newIdx].color));
        } else {
            strlcpy(v_hostsCfg.hosts[0].addr, v_addr,
                    sizeof(v_hostsCfg.hosts[0].addr));
            if (v_label) strlcpy(v_hostsCfg.hosts[0].label, v_label,
                                 sizeof(v_hostsCfg.hosts[0].label));
            if (v_color) strlcpy(v_hostsCfg.hosts[0].color, v_color,
                                 sizeof(v_hostsCfg.hosts[0].color));
        }
    }

    const bool v_saved = _cfg->saveHosts(v_hostsCfg);
    if (v_saved) {
        _sendOk(req, "ok", "Host label saved");
    } else {
        _sendErr(req, "save_failed", "Failed to save hosts config");
    }
}
