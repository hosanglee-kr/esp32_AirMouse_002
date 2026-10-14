/* =======================================================
   File: /www/lib/am_host_0415.js
   Elite AirMouse WebConfig v0415 — Multi-Host Manager Module
   - 로드 순서: 9 (신규 모듈)
   - 기능:
     · /api/hosts 연동 (본딩된 호스트 목록, 활성/선택 상태 조회)
     · 1-Click 호스트 전환 요청 (POST /api/hosts/select)
     · 호스트 전환 취소 요청 (POST /api/hosts/cancel)
     · 호스트 사용자 라벨 및 고유 색상 저장 (POST /api/hosts/label)
     · 상태 동기화 및 뱃지/버튼 실시간 갱신
   ======================================================= */

let g_hostData = null;
let g_hostPollTimer = null;

/* =======================================================
   1. API 통신 래퍼
   ======================================================= */
async function hostFetchList() {
  const r = await apiGet("/api/hosts");
  const u = unwrapApi(r);
  if (u.ok && u.data) {
    g_hostData = u.data;
    renderHostCard(g_hostData);
    return g_hostData;
  }
  return null;
}

async function hostSelect(idx) {
  setLoading(true, t("host.toast_select"));
  const r = await apiPostJson("/api/hosts/select", { idx: idx });
  setLoading(false);
  const u = unwrapApi(r);
  if (u.ok) {
    setMsg(t("host.toast_select"), true);
    await hostFetchList();
    _startHostSelectingPoll();
  } else {
    setMsg(u.msg || "Host select failed", false);
  }
}

async function hostCancel() {
  setLoading(true, t("host.toast_cancel"));
  const r = await apiPostJson("/api/hosts/cancel", {});
  setLoading(false);
  const u = unwrapApi(r);
  if (u.ok) {
    setMsg(t("host.toast_cancel"), true);
    await hostFetchList();
    _stopHostSelectingPoll();
  } else {
    setMsg(u.msg || "Cancel failed", false);
  }
}

async function hostSaveLabel(addr, label, color) {
  const r = await apiPostJson("/api/hosts/label", { addr: addr, label: label, color: color });
  const u = unwrapApi(r);
  if (u.ok) {
    setMsg(t("host.toast_saved"), true);
    await hostFetchList();
  } else {
    setMsg(u.msg || "Label save failed", false);
  }
}

/* =======================================================
   2. 폴링 제어 (선택 중일 때 1초 간격 갱신)
   ======================================================= */
function _startHostSelectingPoll() {
  if (g_hostPollTimer) return;
  g_hostPollTimer = setInterval(async () => {
    const data = await hostFetchList();
    if (!data || !data.is_selecting) {
      _stopHostSelectingPoll();
    }
  }, 1000);
}

function _stopHostSelectingPoll() {
  if (g_hostPollTimer) {
    clearInterval(g_hostPollTimer);
    g_hostPollTimer = null;
  }
}

/* =======================================================
   3. UI 렌더링
   ======================================================= */
function renderHostCard(data) {
  const container = qs("hostListContainer");
  if (!container) return;

  if (!data || !data.hosts || data.hosts.length === 0) {
    container.innerHTML = `<div class="hint2" style="text-align:center;padding:16px;" data-i18n="host.no_hosts">${t("host.no_hosts")}</div>`;
    return;
  }

  container.innerHTML = "";

  const isSelecting = !!data.is_selecting;
  const targetIdx = data.target_idx;

  data.hosts.forEach((h) => {
    const item = document.createElement("div");
    item.className = "host-item";
    if (h.is_active) item.classList.add("host-active");
    if (h.is_target) item.classList.add("host-target");

    const colorDot = document.createElement("span");
    colorDot.className = "host-color-dot";
    colorDot.style.backgroundColor = h.color || "#3B82F6";

    const info = document.createElement("div");
    info.className = "host-info";

    const nameRow = document.createElement("div");
    nameRow.className = "host-name-row";

    const labelSpan = document.createElement("span");
    labelSpan.className = "host-label";
    labelSpan.textContent = h.label && h.label.length > 0 ? h.label : `Host ${h.idx + 1}`;

    const idxBadge = document.createElement("span");
    idxBadge.className = "host-idx-badge";
    idxBadge.textContent = `#${h.idx + 1}`;

    nameRow.appendChild(colorDot);
    nameRow.appendChild(labelSpan);
    nameRow.appendChild(idxBadge);

    if (h.is_active) {
      const activeBadge = document.createElement("span");
      activeBadge.className = "host-status-badge active";
      activeBadge.textContent = `● ${t("host.badge_active")}`;
      nameRow.appendChild(activeBadge);
    } else if (h.is_target) {
      const targetBadge = document.createElement("span");
      targetBadge.className = "host-status-badge target";
      targetBadge.textContent = `⏳ ${t("host.badge_target")}`;
      nameRow.appendChild(targetBadge);
    }

    const addrSpan = document.createElement("div");
    addrSpan.className = "host-addr";
    addrSpan.textContent = h.addr;

    info.appendChild(nameRow);
    info.appendChild(addrSpan);

    const actions = document.createElement("div");
    actions.className = "host-actions";

    // 이름 수정 버튼
    const btnEdit = document.createElement("button");
    btnEdit.className = "btn mini";
    btnEdit.textContent = "✏️";
    btnEdit.title = t("host.edit_label");
    btnEdit.onclick = () => _openHostEditModal(h);
    actions.appendChild(btnEdit);

    // 전환 / 취소 버튼
    if (h.is_target) {
      const btnCancel = document.createElement("button");
      btnCancel.className = "btn mini warn";
      btnCancel.textContent = t("host.btn_cancel");
      btnCancel.onclick = () => hostCancel();
      actions.appendChild(btnCancel);
    } else if (!h.is_active) {
      const btnSel = document.createElement("button");
      btnSel.className = "btn mini primary";
      btnSel.textContent = t("host.btn_select");
      btnSel.onclick = () => hostSelect(h.idx);
      actions.appendChild(btnSel);
    }

    item.appendChild(info);
    item.appendChild(actions);
    container.appendChild(item);
  });
}

function _openHostEditModal(host) {
  const newLabel = prompt(t("host.edit_label") + ` (${host.addr})`, host.label || "");
  if (newLabel !== null) {
    const trimmed = newLabel.trim();
    hostSaveLabel(host.addr, trimmed, host.color || "#3B82F6");
  }
}

/* =======================================================
   4. 초기화
   ======================================================= */
function initHostModule() {
  hostFetchList();
}
