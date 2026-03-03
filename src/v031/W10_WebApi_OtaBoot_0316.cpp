// =======================================================
// File: W10_WebApi_OtaBoot_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_OtaBoot_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: OTA + SafeBoot/FactoryReset/Reboot)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) OTA 업로드/상태 + SafeBoot/FactoryReset/Reboot 분리
 * ------------------------------------------------------
 * [구현 규칙] ... (동일)
 * ------------------------------------------------------
 * [코드 네이밍 규칙] ... (동일)
 * ------------------------------------------------------
 */

#include "W10_Web_0315.h"

// 붙여넣기 대상:
// - apiOtaUpload
// - apiOtaStatus
// - apiSafeBootGet / apiSafeBootPost
// - apiFactoryReset
// - _taskReboot
// - apiRebootPost
// - apiRebootCheck
