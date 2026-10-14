/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Hosts_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Multi-Host API)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/hosts        : 호스트 목록 및 현재 활성/선택 상태 조회 (GET)
 *  - /api/hosts/select : 목표 호스트 지정 전환 요청 (POST)
 *  - /api/hosts/cancel : 호스트 전환 모드 즉시 취소 (POST)
 *  - /api/hosts/label  : 사용자 정의 호스트 라벨 및 색상 저장 (POST)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - CONTRACT §3 준수: Web Task에서 BLE 직접 제어 금지, requestHostOp() 위임
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"
#include <NimBLEDevice.h>

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
        const char* v_color = "#3B82F6"; // 기본 블루
        for (uint8_t h = 0; h < v_hostsCfg.count; h++) {
            if (strcasecmp(v_hostsCfg.hosts[h].addr, v_strAddr.c_str()) == 0) {
                v_label = v_hostsCfg.hosts[h].label;
                v_color = v_hostsCfg.hosts[h].color;
                break;
            }
        }
        v_item["label"] = v_label;
        v_item["color"] = v_color;
    }

    _sendJsonStream(req, v_doc, 200);
}

// =====================================================
// POST /api/hosts/select
// =====================================================
void CL_W10_WebConfig::apiHostsSelect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON body");
        return;
    }

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
// POST /api/hosts/cancel
// =====================================================
void CL_W10_WebConfig::apiHostsCancel(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
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
void CL_W10_WebConfig::apiHostsLabel(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON body");
        return;
    }

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

    // 기존 항목 탐색 또는 신규 추가
    int8_t v_matchIdx = -1;
    for (uint8_t i = 0; i < v_hostsCfg.count; i++) {
        if (strcasecmp(v_hostsCfg.hosts[i].addr, v_addr) == 0) {
            v_matchIdx = i;
            break;
        }
    }

    if (v_matchIdx >= 0) {
        if (v_label) strlcpy(v_hostsCfg.hosts[v_matchIdx].label, v_label, sizeof(v_hostsCfg.hosts[v_matchIdx].label));
        if (v_color) strlcpy(v_hostsCfg.hosts[v_matchIdx].color, v_color, sizeof(v_hostsCfg.hosts[v_matchIdx].color));
    } else {
        if (v_hostsCfg.count < C10_DEF::HOSTS_MAX) {
            uint8_t v_newIdx = v_hostsCfg.count++;
            strlcpy(v_hostsCfg.hosts[v_newIdx].addr, v_addr, sizeof(v_hostsCfg.hosts[v_newIdx].addr));
            if (v_label) strlcpy(v_hostsCfg.hosts[v_newIdx].label, v_label, sizeof(v_hostsCfg.hosts[v_newIdx].label));
            else         strlcpy(v_hostsCfg.hosts[v_newIdx].label, "Host",  sizeof(v_hostsCfg.hosts[v_newIdx].label));
            if (v_color) strlcpy(v_hostsCfg.hosts[v_newIdx].color, v_color, sizeof(v_hostsCfg.hosts[v_newIdx].color));
            else         strlcpy(v_hostsCfg.hosts[v_newIdx].color, "#3B82F6", sizeof(v_hostsCfg.hosts[v_newIdx].color));
        } else {
            // 가득 찬 경우 첫번째 항목 교체
            strlcpy(v_hostsCfg.hosts[0].addr, v_addr, sizeof(v_hostsCfg.hosts[0].addr));
            if (v_label) strlcpy(v_hostsCfg.hosts[0].label, v_label, sizeof(v_hostsCfg.hosts[0].label));
            if (v_color) strlcpy(v_hostsCfg.hosts[0].color, v_color, sizeof(v_hostsCfg.hosts[0].color));
        }
    }

    const bool v_saved = _cfg->saveHosts(v_hostsCfg);
    if (v_saved) {
        _sendOk(req, "ok", "Host label saved");
    } else {
        _sendErr(req, "save_failed", "Failed to save hosts config");
    }
}
