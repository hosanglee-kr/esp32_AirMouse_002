/* =======================================================
   File: /www/lib/am_offline_store_0415.js
   Elite AirMouse WebConfig v0415 — Offline Store
   -------------------------------------------------------
   소스명 : am_offline_store_0415.js
   모듈약어 : OFFLINE_STORE
   모듈명 : Offline Simulator - localStorage Store
   -------------------------------------------------------
   기능 요약
    - 오프라인 시뮬레이터 스토어 관리 (localStorage 전담)
      · OFFLINE_STORAGE_KEY : 세션 저장 키 ("airmouse_v0415_offline_store")
      · OFFLINE_STORE_SCHEMA: 내부 스키마 (6)
      · g_offlineStore      : 런타임 스토어 상태
      · loadOfflineStore()  : 초기화 (스키마 불일치 시 자동 reset)
      · saveOfflineStore()  : 영속화 (online 모드에서는 no-op)

   [v0415]
    - STORAGE_KEY 갱신: airmouse_v0412 → airmouse_v0415
      · 스키마 변경(v410→v411)으로 이전 스토어 자동 초기화 유도
    - OFFLINE_STORE_SCHEMA = 6 (v0415 유지, 내부 스키마 별도)

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 모듈은 am_offline_0415.js에서 스토어 파트 분리
      · am_offline_data_0415.js   : 목업 데이터 (G_OFFLINE_*)
      · am_offline_store_0415.js  : 본 파일 (localStorage 스토어)
      · am_offline_0415.js        : handleOfflineApi + 오케스트레이션
    - 로드 순서 SSOT:
      am_base → am_i18n → am_offline_data → am_offline_store → am_offline
    - G_OFFLINE_DEFAULT_PROFILE_0 의존 (data 모듈)

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사 (G_OFFLINE_*)
    - 전역 변수           : g_모듈약어_ 접두사 (g_offlineStore)
    - 함수                : camelCase
    - 로컬 변수           : v_ 접두사
   ======================================================= */

/* =======================================================
   오프라인 로컬 저장소
   - [v0415] STORAGE_KEY 갱신: airmouse_v0412 → airmouse_v0415
     · 스키마 변경(v410→v411)으로 이전 스토어 자동 초기화 유도
   ======================================================= */
const OFFLINE_STORAGE_KEY = "airmouse_v0415_offline_store";
const OFFLINE_STORE_SCHEMA = 6; // v0415 유지 (내부 스키마 별도)

let g_offlineStore = null;

function loadOfflineStore() {
  try {
    const raw = localStorage.getItem(OFFLINE_STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (parsed &&
        typeof parsed === "object" &&
        parsed._v === OFFLINE_STORE_SCHEMA &&
        parsed.profiles) {
        g_offlineStore = parsed;
        return;
      }
      console.info("[offline] storage schema mismatch → reset",
        "found:", parsed && parsed._v,
        "expected:", OFFLINE_STORE_SCHEMA);
    }
  } catch (e) {}
  
  g_offlineStore = {
    _v: OFFLINE_STORE_SCHEMA,
    active: 0,
    profiles: [{ idx: 0, name: "Default" }],
    profileData: {
      0: JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0))
    }
  };
}

function saveOfflineStore() {
  if (g_appMode !== APP_MODE_OFFLINE) return;
  
  try {
    if (g_offlineStore) {
      localStorage.setItem(OFFLINE_STORAGE_KEY, JSON.stringify(g_offlineStore));
    }
  } catch (e) {}
}
