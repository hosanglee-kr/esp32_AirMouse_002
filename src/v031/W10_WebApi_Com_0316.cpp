// =======================================================
// File: W10_WebApi_Com_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Com_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: API Common Helpers/Policies)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) API 공통 유틸/정책 분리
 *  - JSON Stream helper, ETag/304(API), SafeMode Gate, reboot helper 등
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버 함수/변수   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include "W10_Web_0315.h"

// 여기에 아래 함수들을 "그대로 잘라서" 붙여넣기:
// - _sendJsonStream
// - _resPrintJsonString(2)
// - _formatEtagQuoted
// - _ifNoneMatchHit
// - _send304NoStoreEtag
// - _sendOk / _httpFromCode / _sendErr
// - _isSafeMode / _isApiAllowedInSafeMode / _gateSafeModeOrReply
// - _wantsEnvelope
// - _wifiDiffMask / _markNeedReboot / _markLastApply / _rebootReasonsString


// =====================================================
// Common JSON response helper (API는 무조건 no-store)
// =====================================================
void CL_W10_WebConfig::_sendJsonStream(AsyncWebServerRequest* req, JsonDocument& d, int p_code) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(p_code);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    serializeJson(d, *res);
    req->send(res);
}



// =====================================================
// 200 공통 (API/config/export): no-store + ETag + X-Config-Size
// - 정적과 달리 gzip variant가 없으므로 Vary 필요 없음(정적만 Vary 표준화)
// =====================================================
void CL_W10_WebConfig::_addEtagHeadersNoStore(AsyncWebServerResponse* res,
                                             bool p_hasEtag,
                                             uint32_t p_etag,
                                             size_t p_size) {
    if (!res) return;

    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);

    if (p_hasEtag) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)p_size));
    }
}



