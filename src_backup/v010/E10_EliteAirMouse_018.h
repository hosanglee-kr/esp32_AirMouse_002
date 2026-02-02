#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_018.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (Composite HID, Status/Control v0.1.8, ModifierPolicyFix)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균, 움직임 큰 샘플 제외)
 *  - BTN_SCROLL 전용 버튼 분리(스크롤/기본 조작 충돌 제거)
 *  - Hard Click-Lock 옵션(150ms 완전 고정) : *M10_MotionProc_015.h*
 *  - /api/status 제공용 상태 구조체(gyro bias/temp/sampling/err 카운터 등)
 *  - 웹에서 PPT 모드 토글 / DPI 즉시 변경 지원
 *  - ✅ Modifier 전송 정책 확정:
 *     - W10 /api/keycodes 의 mods.mask = HID modifier bitfield(보고서의 modifier byte)
 *     - E10은 mask를 변환하지 않고 그대로 modifierKeyPress(mask)로 전송
 *     - OS 호환/조합키(Ctrl+Shift 등) 안정성이 가장 높음
 *
 * ------------------------------------------------------
 * [튜닝 TIP 요약]
 *  - (A) Flick 제스처 민감도:
 *      gesture_flick_deg ↑ : 둔감(오작동↓) / ↓ : 민감(손목 튕김에 잘 반응)
 *      gesture_cooldown_ms ↑ : 연속 오작동↓ / ↓ : 빠른 넘김↑
 *  - (B) 가속 체감:
 *      accel_threshold ↓ : 작은 이동에도 가속 빨리 붙음
 *      accel_gain[dpi] ↑ : 큰 이동에서 더 “확” 뻗음
 *  - (C) Scroll 모드 커서 흔들림:
 *      scroll_cursor_damp ↓(예:0.25→0.15) : 스크롤 중 커서 더 고정(대신 이동 어려움)
 *  - (D) 스크롤 방향/축:
 *      mouseSend()에서 mouseMove(dx,dy,scrollX,scrollY) 중 scrollX/scrollY를 바꿔
 *      PC에서 느껴지는 스크롤 축(세로/가로) mismatch를 해결 가능
 *  - (E) 조합키 stuck(키가 남음) 느낌:
 *      tapComboUsage() 끝에 resetKeys()를 “PPT 명령 전용”으로 옵션 적용 가능
 *      (항상 호출하면 입력감이 둔해질 수 있음)
 *
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
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "M10_MotionProc_015.h"
#include "C10_Config_017.h"

typedef bool (*T_E10_ApplyFn)(void* p_ctx);

struct ST_E10_Status_t {
    // runtime
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;

    // sensor
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float temp_c;

    // timing
    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    // counters
    uint32_t err_mpu_read;
    uint32_t err_mutex_miss;
    uint32_t err_task_overrun;

    // misc
    uint32_t uptime_ms;
};

class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config* _cfg = nullptr;

    // class static members -> s_ prefix
    static constexpr int s_btnL      = 12;
    static constexpr int s_btnMode   = 13;
    static constexpr int s_btnScroll = 14;

    volatile bool _isPptMode = false;

    struct ST_E10_State_t {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t s_mouseBtnLeft = 0x01;

    // gyro bias (deg/s)
    float _gyroBiasX = 0.0f;
    float _gyroBiasY = 0.0f;
    float _gyroBiasZ = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t s_calibMs = 1000;
    static constexpr float    s_calibStillThDeg = 3.0f;

    // config runtime
    int   _dpiLevel = 2;
    bool  _hardClickLock = true;

    float _scaleBase[3] = {0.55f, 0.75f, 1.00f};
    float _accelGain[3] = {0.35f, 0.55f, 0.85f};
    float _accelTh = 8.0f;

    float _wheelThDeg = 90.0f;
    int   _wheelStepMax = 6;

    float    _gestureFlickDeg = 200.0f;
    uint16_t _gestureCooldownMs = 600;

    float _scrollCursorDamp = 0.25f;

    ST_C10_PptKey_t _pptStart;
    ST_C10_PptKey_t _pptExit;
    ST_C10_PptKey_t _pptNext;
    ST_C10_PptKey_t _pptPrev;
    ST_C10_PptKey_t _pptBlack;
    ST_C10_PptKey_t _pptLaser;

    // status vars
    float _tempC = 0.0f;
    uint32_t _uptime0 = 0;

    // timing avg
    float _dtAvgMs = 8.0f;
    uint32_t _dtAvgCnt = 0;

    // counters
    uint32_t _errMpuRead = 0;
    uint32_t _errMutexMiss = 0;
    uint32_t _errTaskOverrun = 0;

    // PPT combo 안정화 옵션(키 stuck 방지)
    // - true면 tapKey/tapCombo 후 resetKeys()를 추가 호출
    // - TIP: 항상 true면 입력감이 “둔”해질 수 있어, PPT 명령에만 적용 권장
    bool _pptResetKeysAfterTap = false;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state = {0,0,0,false};
        memset(&_pptStart, 0, sizeof(_pptStart));
        memset(&_pptExit,  0, sizeof(_pptExit));
        memset(&_pptNext,  0, sizeof(_pptNext));
        memset(&_pptPrev,  0, sizeof(_pptPrev));
        memset(&_pptBlack, 0, sizeof(_pptBlack));
        memset(&_pptLaser, 0, sizeof(_pptLaser));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;
        _uptime0 = millis();

        Serial.begin(115200);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("Failed to find MPU6050 chip");
            for (;;) delay(10);
        }

        // [튜닝 TIP] GyroRange를 올리면(500/1000deg) 빠른 회전에서 clip이 줄지만 미세 조작이 거칠어질 수 있음
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);

        // [튜닝 TIP] 2G가 일반적. 4G 이상은 큰 충격에도 덜 saturate 되지만 노이즈/해상도 체감 변화 가능
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);

        // [튜닝 TIP] 21Hz는 손 떨림 억제와 지연의 절충. 더 낮추면 부드럽지만 느려짐
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(s_btnL, INPUT_PULLUP);
        pinMode(s_btnMode, INPUT_PULLUP);
        pinMode(s_btnScroll, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        (void)applyFromConfig();

        // Composite HID 구성
        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    // 기존 Apply 콜백
    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // (3) 웹에서 PPT/DPI 즉시 변경
    bool setPptMode(bool p_enable) {
        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);
        _isPptMode = p_enable;
        if (_mutex != nullptr) xSemaphoreGive(_mutex);
        return true;
    }

    bool setDpiLevel(uint8_t p_level) {
        if (p_level < 1) p_level = 1;
        if (p_level > 3) p_level = 3;

        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);
        _dpiLevel = (int)p_level;
        _engine.setDPI(_dpiLevel);
        if (_mutex != nullptr) xSemaphoreGive(_mutex);
        return true;
    }

    // /api/status
    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);

        p_out.ble_connected = _hid.isConnected();
        p_out.ppt_mode = _isPptMode;
        p_out.dpi_level = (uint8_t)_dpiLevel;

        p_out.gyro_bias_x = _gyroBiasX;
        p_out.gyro_bias_y = _gyroBiasY;
        p_out.gyro_bias_z = _gyroBiasZ;
        p_out.temp_c = _tempC;

        p_out.sampling_ms_target = 8;
        p_out.sampling_ms_avg = _dtAvgMs;

        p_out.err_mpu_read = _errMpuRead;
        p_out.err_mutex_miss = _errMutexMiss;
        p_out.err_task_overrun = _errTaskOverrun;

        p_out.uptime_ms = (uint32_t)(millis() - _uptime0);

        if (_mutex != nullptr) xSemaphoreGive(_mutex);
    }

  private:
    bool applyFromConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        memset(&v_w, 0, sizeof(v_w));
        memset(&v_e, 0, sizeof(v_e));

        (void)_cfg->loadAll(v_w, v_e);

        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);

        _dpiLevel = (int)v_e.dpi_level;
        _hardClickLock = v_e.hard_click_lock;

        _scaleBase[0] = v_e.scale_base[0];
        _scaleBase[1] = v_e.scale_base[1];
        _scaleBase[2] = v_e.scale_base[2];

        _accelGain[0] = v_e.accel_gain[0];
        _accelGain[1] = v_e.accel_gain[1];
        _accelGain[2] = v_e.accel_gain[2];

        _accelTh = v_e.accel_threshold;

        _wheelThDeg = v_e.wheel_threshold_deg;
        _wheelStepMax = (int)v_e.wheel_step_max;

        _gestureFlickDeg = v_e.gesture_flick_deg;
        _gestureCooldownMs = v_e.gesture_cooldown_ms;

        _scrollCursorDamp = v_e.scroll_cursor_damp;

        _pptStart = v_e.ppt_start;
        _pptExit  = v_e.ppt_exit;
        _pptNext  = v_e.ppt_next;
        _pptPrev  = v_e.ppt_prev;
        _pptBlack = v_e.ppt_black;
        _pptLaser = v_e.ppt_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);

        if (_mutex != nullptr) xSemaphoreGive(_mutex);

        Serial.printf("[E10] apply ok: dpi=%d hard=%d accelTh=%.2f wheelTh=%.1f step=%d flick=%.1f cd=%u damp=%.2f\n",
                      _dpiLevel, _hardClickLock ? 1 : 0, _accelTh, _wheelThDeg, _wheelStepMax,
                      _gestureFlickDeg, (unsigned)_gestureCooldownMs, _scrollCursorDamp);

        return true;
    }

    // ------------------------------------------------------
    // Keyboard sending policy
    // ------------------------------------------------------
    // ✅ 정책 확정:
    //  - p_modMask는 HID modifier bitfield(KeyboardInputReport.modifiers)에 들어갈 값
    //  - W10 /api/keycodes 의 mods.mask 를 그대로 사용 (변환 없음)
    //
    // 예) Ctrl+Shift = 0x01|0x02 = 0x03
    //
    // 장점:
    //  - 표준에 가장 부합
    //  - 조합키(다중 modifier OR) 안정
    //  - OS 호환성이 가장 높음
    void tapKeyUsage(uint8_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);

        // [튜닝 TIP] 키가 남는(stuck) 현상이 있으면 아래 옵션 적용
        if (_pptResetKeysAfterTap) _keyboard.resetKeys();
    }

    void tapComboUsage(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 18) {
        if (p_usage == 0) return;

        if (p_modMask == 0) {
            tapKeyUsage(p_usage, p_ms);
            return;
        }

        // ✅ modifier byte 전송(보고서 bitfield)
        _keyboard.modifierKeyPress(p_modMask);
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
        _keyboard.modifierKeyRelease(p_modMask);

        // [튜닝 TIP] PPT 명령에서만 stuck이 가끔 있으면 true로(항상 true는 입력감 둔해질 수 있음)
        if (_pptResetKeysAfterTap) _keyboard.resetKeys();
    }

    void sendPptKey(const ST_C10_PptKey_t& p_k) {
        if (!_hid.isConnected()) return;
        if (p_k.key == 0) return;

        // [TIP] PPT 명령만 resetKeys 옵션을 켜고 싶으면, 여기에서만 임시로 true로 바꿔도 됨
        // bool v_prev = _pptResetKeysAfterTap;
        // _pptResetKeysAfterTap = true;

        if (p_k.mod == 0) tapKeyUsage((uint8_t)p_k.key);
        else tapComboUsage((uint8_t)p_k.mod, (uint8_t)p_k.key, 22);

        // _pptResetKeysAfterTap = v_prev;
    }

    void processGesturesDeg(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < _gestureCooldownMs) return;

        // [튜닝 TIP] flick 임계값을 올리면 오작동 감소(둔감), 내리면 민감
        if (p_gzDegPerSec > _gestureFlickDeg) { sendPptKey(_pptPrev); s_lastFlick = millis(); }
        else if (p_gzDegPerSec < -_gestureFlickDeg) { sendPptKey(_pptNext); s_lastFlick = millis(); }
    }

    // ------------------------------------------------------
    // Mouse send policy
    // ------------------------------------------------------
    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        // MouseDevice::mouseMove(x,y,scrollX,scrollY)
        //
        // [튜닝 TIP] 스크롤 방향/축 mismatch 해결:
        //  - 세로 스크롤로 쓰고 싶으면 scrollY에 넣는 것이 일반적일 수 있음
        //    p_ms.mouseMove(p_dx, p_dy, 0, p_wheel);
        //  - 현재 구현은 scrollX에 wheel 사용(기존 유지)
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // ------------------------------------------------------
    // Gyro calibration policy
    // ------------------------------------------------------
    void runGyroCalibration() {
        const uint32_t v_t0 = millis();
        uint32_t v_cnt = 0;
        double v_sumX = 0.0, v_sumY = 0.0, v_sumZ = 0.0;

        // [튜닝 TIP]
        //  - s_calibMs를 늘리면 평균이 더 안정적(대신 부팅 대기 증가)
        //  - s_calibStillThDeg를 낮추면 "정말 고정된 샘플만" 사용(엄격)
        while (millis() - v_t0 < s_calibMs) {
            sensors_event_t v_a, v_g, v_temp;
            _mpu.getEvent(&v_a, &v_g, &v_temp);

            const float v_gx = v_g.gyro.x * RAD_TO_DEG;
            const float v_gy = v_g.gyro.y * RAD_TO_DEG;
            const float v_gz = v_g.gyro.z * RAD_TO_DEG;

            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < s_calibStillThDeg) {
                v_sumX += v_gx; v_sumY += v_gy; v_sumZ += v_gz; v_cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (v_cnt > 0) {
            _gyroBiasX = (float)(v_sumX / (double)v_cnt);
            _gyroBiasY = (float)(v_sumY / (double)v_cnt);
            _gyroBiasZ = (float)(v_sumZ / (double)v_cnt);
        }

        _gyroCalibDone = true;
        Serial.printf("[E10] gyro calib: bx=%.3f by=%.3f bz=%.3f samples=%u\n",
                      _gyroBiasX, _gyroBiasY, _gyroBiasZ, (unsigned)v_cnt);
    }

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        if (!v_m->_gyroCalibDone) v_m->runGyroCalibration();

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);
            v_m->_tempC = v_temp.temperature;

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            // dt 평균(상태 표시용)
            const float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgCnt++;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

            // Scroll 모드: BTN_SCROLL을 누르는 동안만 스크롤
            const bool v_scrollMode = (digitalRead(s_btnScroll) == LOW);

            // mode 버튼: short=dpi cycle, long=ppt toggle
            if (digitalRead(s_btnMode) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) v_m->_isPptMode = !v_m->_isPptMode;
                    else {
                        v_m->_dpiLevel++;
                        if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                        v_m->_engine.setDPI(v_m->_dpiLevel);
                    }
                    v_btnDownMs = 0;
                }
            }

            // gyro (deg/s) + bias 제거
            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            // orientation 보정: gx 사용(roll drift 보정용)
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            // PPT 제스처는 스크롤 중에는 차단 권장(오작동 방지)
            if (v_m->_isPptMode && !v_scrollMode) v_m->processGesturesDeg(v_gz);

            const bool v_leftClick = (digitalRead(s_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // DPI scale + accel
            float v_base = v_m->_scaleBase[v_m->_dpiLevel - 1];
            float v_accg = v_m->_accelGain[v_m->_dpiLevel - 1];

            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;

            // [튜닝 TIP] accel_threshold를 낮추면 작은 움직임에도 가속이 빨리 붙음
            if (v_mag > v_m->_accelTh) {
                const float v_ex = (v_mag - v_m->_accelTh);
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            // Scroll 모드: gyro.y로 wheel 생성 + 커서 이동 감쇠
            int v_wheel = 0;
            if (v_scrollMode) {
                // [튜닝 TIP] wheel_threshold_deg를 낮추면 스크롤이 쉽게 발생(민감)
                if (v_gy > v_m->_wheelThDeg) {
                    float v_norm = (v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = (int)(1 + (v_norm * (v_m->_wheelStepMax - 1)));
                } else if (v_gy < -v_m->_wheelThDeg) {
                    float v_norm = (-v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = -(int)(1 + (v_norm * (v_m->_wheelStepMax - 1)));
                }

                // [튜닝 TIP] scroll_cursor_damp ↓ : 스크롤 중 커서 더 고정(대신 이동 어려움)
                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;
            }

            // 공유 상태 저장
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = (int)v_fx;
                v_m->_state.y = (int)v_fy;
                v_m->_state.wheel = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            } else {
                v_m->_errMutexMiss++;
            }

            // 버튼 상태 즉시 반영
            if (v_m->_hid.isConnected()) {
                if (v_leftClick) v_m->_mouse.mousePress(s_mouseBtnLeft);
                else v_m->_mouse.mouseRelease(s_mouseBtnLeft);
            }

            // overrun 감지(대략)
            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8)); // 125Hz
            TickType_t v_after = xTaskGetTickCount();
            if ((v_after - v_before) == 0) {
                // 즉시 리턴 케이스는 "이미 늦음" 가능성이 높음 (대략적 지표)
                v_m->_errTaskOverrun++;
            }
        }
    }

    static void commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_hid.isConnected() && xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (v_m->_state.updated) {
                    const int8_t v_dx = (int8_t)constrain(v_m->_state.x, -127, 127);
                    const int8_t v_dy = (int8_t)constrain(v_m->_state.y, -127, 127);
                    const int8_t v_wh = (int8_t)constrain(v_m->_state.wheel, -127, 127);

                    mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

