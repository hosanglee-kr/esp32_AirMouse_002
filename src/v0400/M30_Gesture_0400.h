// =======================================================
// File: src/v040/M30_Gesture_0400.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M30_Gesture_0400.h
 * 모듈약어 : M30
 * 모듈명 : Gesture Detector (Flick / Linear / Tilt Hold)
 * ------------------------------------------------------
 * 기능 요약
 *  - Flick: Peak-to-Peak + Sign (좌/우/상/하)
 *  - Linear: 선형 가속 임펄스 적분 (4방향, Middle Hold 중만)
 *  - Tilt Hold: 자세 유지 감지 (4방향, Mode 3 Middle Hold 해제 시만)
 *
 * [좌표계 규약]
 *   물리 축 (MPU6050)      사용자 용어     용도
 *   gx                     Roll           커서 Y, Linear U/D
 *   gy                     Pitch          커서 X?, 휠, Linear L/R
 *   gz                     Yaw            커서 X, Flick L/R
 *
 *   accel (g 단위, m/s² 변환은 내부 처리)
 *   ax, ay, az
 *
 * [감지 방식]
 *   Flick (P2P):
 *     200ms 슬라이딩 윈도우 내 peak-to-peak > p2p_th → 부호 판정
 *     쿨다운 600ms
 *
 *   Linear (임펄스 적분):
 *     Linear Accel = raw - 중력성분(Roll/Pitch 회전 반영)
 *     |lin_accel| > th → 윈도우 적분
 *     300ms 윈도우 종료 시 |impulse| > impulse_th → 지배축+부호 판정
 *     Middle Hold 중에만 활성 (config의 move_gate가 REQUIRED면 자동)
 *
 *   Tilt Hold:
 *     Roll/Pitch 자세 > angle_deg 유지 300ms → 방향 판정
 *     3Hz 반복 (hold 중)
 *     Mode 3 + Middle Hold 해제 시만
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명 : CL_모듈약어_ 접두사
 *   - private  : _ 접두사
 *   - 로컬     : v_ 접두사
 *   - 인자     : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <math.h>
#include <string.h>

// ======================================================
// 방향 상수 (4방향 공통)
// ======================================================
enum EN_M30_Dir_t : uint8_t {
    EN_M30_DIR_LEFT  = 0,
    EN_M30_DIR_RIGHT = 1,
    EN_M30_DIR_UP    = 2,
    EN_M30_DIR_DOWN  = 3,
    EN_M30_DIR_NONE  = 0xFF
};

class CL_M30_Gesture {
  public:
    // ==================================================
    // Config
    // ==================================================
    struct ST_Config_t {
        // Flick
        float    flick_p2p_th;        // deg/s
        uint16_t flick_window_ms;     // 200
        uint16_t flick_cooldown_ms;   // 600

        // Linear
        float    linear_th;           // m/s²  (0.3)
        float    linear_impulse_th;   // m/s   (0.5)
        uint16_t linear_window_ms;    // 300

        // Tilt Hold
        float    tilt_angle_deg;      // 15
        uint16_t tilt_hold_ms;        // 300
        uint8_t  tilt_repeat_hz;      // 3
    };

  private:
    ST_Config_t _cfg;

    // ---- Flick: 200ms 슬라이딩 윈도우 ----
    static constexpr uint8_t  G_FLICK_BUF = 32;   // 8ms × 32 = 256ms 커버
    struct {
        float    bufYaw  [G_FLICK_BUF];  // gz
        float    bufRoll [G_FLICK_BUF];  // gx  (상하 Flick용)
        uint8_t  head    = 0;
        uint8_t  count   = 0;
        uint32_t lastFireMs = 0;
    } _flick;

    // ---- Linear: 임펄스 적분 ----
    struct {
        // 중력 성분 제거용 자세 스냅샷 (roll/pitch in rad)
        float    roll  = 0.0f;
        float    pitch = 0.0f;

        // 축별 임펄스 (m/s)
        float    accumRoll = 0.0f;    // gx축 임펄스
        float    accumPitch = 0.0f;   // gy축 임펄스 (커서 X?)

        // 300ms 윈도우
        uint32_t windowStartMs = 0;
        bool     windowOpen    = false;
    } _linear;

    // ---- Tilt Hold ----
    struct {
        float    lastRoll  = 0.0f;
        float    lastPitch = 0.0f;

        EN_M30_Dir_t activeDir = EN_M30_DIR_NONE;
        uint32_t     dirStartMs = 0;
        uint32_t     lastRepeatMs = 0;
        bool         fired = false;
    } _tilt;

  public:
    CL_M30_Gesture();

    // Config 적용
    void setConfig(const ST_Config_t& p_cfg) { _cfg = p_cfg; }
    const ST_Config_t& getConfig() const { return _cfg; }

    // ==================================================
    // 매 프레임 호출 (sensorTask)
    //   p_gx/p_gy/p_gz  : bias-corrected gyro (deg/s)
    //   p_ax/p_ay/p_az  : raw accel (g)
    //   p_roll/p_pitch  : 상보 필터 자세 (rad)
    //   p_moveGateHeld  : Top M Hold 상태
    //   p_mode          : active mode (1/2/3)
    // ==================================================
    struct ST_Output_t {
        EN_M30_Dir_t flick  = EN_M30_DIR_NONE;  // 이번 프레임 발생 (없으면 NONE)
        EN_M30_Dir_t linear = EN_M30_DIR_NONE;
        EN_M30_Dir_t tilt   = EN_M30_DIR_NONE;
    };

    ST_Output_t update(float p_gx, float p_gy, float p_gz,
                       float p_ax, float p_ay, float p_az,
                       float p_roll, float p_pitch,
                       bool  p_moveGateHeld,
                       uint8_t p_mode,
                       uint32_t p_nowMs);

    // 리셋 (Mode 전환, 재캘리브 등)
    void reset();

  private:
    // Flick 내부
    EN_M30_Dir_t _detectFlickYaw(float p_gz, uint32_t p_nowMs);
    EN_M30_Dir_t _detectFlickRoll(float p_gx, uint32_t p_nowMs);

    // Linear 내부
    EN_M30_Dir_t _detectLinear(float p_gx, float p_gy,
                               float p_ax, float p_ay, float p_az,
                               float p_roll, float p_pitch,
                               uint32_t p_nowMs);

    // Tilt 내부
    EN_M30_Dir_t _detectTilt(float p_roll, float p_pitch,
                             bool p_active,
                             uint32_t p_nowMs);

    // 버퍼 helper
    void _flickPush(float p_yaw, float p_roll);
    void _flickPeakToPeak(float& p_outYawMin, float& p_outYawMax,
                          float& p_outRollMin, float& p_outRollMax) const;
};
