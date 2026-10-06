// =======================================================
// File: src/LD2420.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420.cpp
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 레이더 센서 메인 컨트롤러 구현부
 * ------------------------------------------------------
 * 기능 요약
 *  - FreeRTOS 듀얼 태스크(rxTask, cmdTask)를 통한 비동기 UART 통신 구현
 *  - rxTask: UART 데이터 배치 읽기, 바이너리/텍스트 프레임 디코딩, FSM 전이, 콜백 호출
 *  - cmdTask: 비동기 명령 큐 처리, ACK 타임아웃 감시(1000ms), 자동 재전송(최대 3회)
 *  - FSM 상태 관리: STARTUP → LISTEN_SETTLE → LISTEN → RUN 및 CONFIG, CALIBRATE
 *  - 최신 측정 데이터 뮤텍스 동기화 및 이벤트 디스패치
 *
 * [설계]
 *  - 논블로킹 아키텍처: delay() 루프 없이 모든 작업은 FreeRTOS 틱 지연 및 FSM 타이머로 처리
 *  - Core 0 격리: ESP32의 코어 0에 rxTask(우선순위 5) 및 cmdTask(우선순위 4)를 고정하여
 *    코어 1에서 실행되는 메인 루프(AirMouse 마우스/센서 파이프라인)의 지연을 원천 차단
 *  - ESPHome 호환성: 부팅 시퀀스, 수신 배치 버퍼링, ACK 타임아웃, 캘리브레이션 수식 일치
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 클래스 메서드       : LD2420:: 접두사 + camelCase
 *  - 멤버 변수           : _ 접두사 + camelCase
 *  - FreeRTOS 핸들       : _ 접두사 + camelCase
 * ------------------------------------------------------
 */

#include "LD2420.h"

// =======================================================
// [생성자 / 소멸자]
// =======================================================

/**
 * @brief LD2420 센서 컨트롤러 생성자
 * @param serial 사용할 하드웨어 시리얼 참조 (Serial1, Serial2 등)
 * @param rxPin ESP32 수신 핀 (센서의 TX 핀과 연결)
 * @param txPin ESP32 송신 핀 (센서의 RX 핀과 연결)
 * @param baud 통신 속도 (기본값 115200bps)
 */
LD2420::LD2420(HardwareSerial& serial, uint8_t rxPin, uint8_t txPin, uint32_t baud)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud) {
    memset(&_latest, 0, sizeof(_latest));
}

/**
 * @brief LD2420 센서 소멸자
 * end()를 호출하여 구동 중인 태스크 및 IPC 리소스를 정리합니다.
 */
LD2420::~LD2420() {
    end();
}

// =======================================================
// [초기화 및 리소스 해제]
// =======================================================

/**
 * @brief 센서 시리얼 포트 오픈 및 FreeRTOS 듀얼 태스크/IPC 객체 생성
 * @param rxStack rxTask에 할당할 스택 크기 (바이트, 기본 4096)
 * @param cmdStack cmdTask에 할당할 스택 크기 (바이트, 기본 3072)
 * @param rxPrio rxTask 우선순위 (기본 5, 실시간 UART 수신 보장)
 * @param cmdPrio cmdTask 우선순위 (기본 4, 명령 송신 및 재시도)
 * @return true: 모든 리소스 생성 성공 및 초기화 완료
 */
bool LD2420::begin(uint32_t rxStack, uint32_t cmdStack, UBaseType_t rxPrio, UBaseType_t cmdPrio) {
    // 1. 하드웨어 시리얼 포트 초기화
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(50);
    // 버퍼에 남아있는 초기 쓰레기 바이트 비우기 (flush)
    while (_serial.available()) _serial.read();

    // 2. FreeRTOS 동기화 IPC 객체 생성
    _dataMutex = xSemaphoreCreateMutex();
    _cmdQueue  = xQueueCreate(LD2420_CMD_QUEUE_SIZE, sizeof(LD2420CmdFrame));
    _events    = xEventGroupCreate();
    if (!_dataMutex || !_cmdQueue || !_events) return false;

    // 3. 초기 FSM 상태를 STARTUP으로 진입
    setState(LD2420State::STARTUP);

    // 4. rxTask 생성: 코어 0에 고정 (메인 루프 및 Wi-Fi와 분리)
    if (xTaskCreatePinnedToCore(rxTaskTrampoline, "ld2420_rx", rxStack, this, rxPrio, &_rxTask, 0) != pdPASS)
        return false;

    // 5. cmdTask 생성: 코어 0에 고정, rxTask보다 낮은 우선순위로 설정
    if (xTaskCreatePinnedToCore(cmdTaskTrampoline, "ld2420_cmd", cmdStack, this, cmdPrio, &_cmdTask, 0) != pdPASS) {
        vTaskDelete(_rxTask);
        _rxTask = nullptr;
        return false;
    }

    // 6. 시작 시퀀스: 센서 상태 확인을 위해 펌웨어 버전 조회 명령 비동기 발행
    requestFirmwareVersion();
    return true;
}

/**
 * @brief 동작 중인 FreeRTOS 태스크를 안전하게 종료하고 모든 동기화 리소스를 파괴
 */
void LD2420::end() {
    // 종료 이벤트 비트 세팅하여 태스크 루프에 종료 신호 전달
    if (_events) xEventGroupSetBits(_events, EV_SHUTDOWN);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 태스크 삭제
    if (_rxTask) {
        vTaskDelete(_rxTask);
        _rxTask = nullptr;
    }
    if (_cmdTask) {
        vTaskDelete(_cmdTask);
        _cmdTask = nullptr;
    }

    // IPC 리소스 삭제
    if (_dataMutex) {
        vSemaphoreDelete(_dataMutex);
        _dataMutex = nullptr;
    }
    if (_cmdQueue) {
        vQueueDelete(_cmdQueue);
        _cmdQueue = nullptr;
    }
    if (_events) {
        vEventGroupDelete(_events);
        _events = nullptr;
    }
}

// =======================================================
// [FreeRTOS 태스크 트램폴린]
// C++ 멤버 함수를 FreeRTOS C형태 콜백에서 호출하기 위한 정적 중계 함수
// =======================================================

void LD2420::rxTaskTrampoline(void* arg) {
    static_cast<LD2420*>(arg)->rxTaskLoop();
}

void LD2420::cmdTaskTrampoline(void* arg) {
    static_cast<LD2420*>(arg)->cmdTaskLoop();
}

// =======================================================
// [RX 수신 태스크 메인 루프]
// =======================================================

/**
 * @brief 데이터 수신 및 프레임 파싱 담당 태스크 (5ms 주기 실행)
 * - ESPHome의 read_batch_ 전략 채택: 수신 가능한 바이트들을 64바이트 청크 단위로 한 번에 읽음
 * - 바이트 단위 핸들러(handleByte)를 거쳐 완전한 프레임 완성 시 디코딩 수행
 * - 상태 머신(processStateMachine)의 시간 기반 전이 관리
 */
void LD2420::rxTaskLoop() {
    const TickType_t period = pdMS_TO_TICKS(5);

    while (true) {
        // 셧다운 이벤트 비트 감시
        if (xEventGroupGetBits(_events) & EV_SHUTDOWN) {
            vTaskDelete(nullptr);
            return;
        }

        // 배치 수신 (Batch Read)
        size_t avail = _serial.available();
        if (avail > 0) {
            uint8_t chunk[64];
            while (avail > 0) {
                size_t toRead = (avail > sizeof(chunk)) ? sizeof(chunk) : avail;
                size_t n      = _serial.readBytes(chunk, toRead);
                if (n == 0) break;
                for (size_t i = 0; i < n; i++) handleByte(chunk[i]);
                avail -= n;
            }
        }

        // FSM 상태 전이 및 타이머 처리
        processStateMachine(millis());
        vTaskDelay(period);
    }
}

// =======================================================
// [FSM 상태 머신 관리]
// =======================================================

/**
 * @brief 센서의 수명주기 및 모드 상태 머신 처리
 * @param now 현재 밀리초 시각 (millis())
 */
void LD2420::processStateMachine(uint32_t now) {
    const uint32_t elapsed = now - _stateStartMs;

    switch (_state) {
        case LD2420State::STARTUP:
            // 부팅 안정화 시간(500ms) 경과 시 데이터 청취(LISTEN) 상태로 전환
            if (elapsed >= LD2420_STARTUP_SETTLE_MS) setState(LD2420State::LISTEN);
            break;

        case LD2420State::LISTEN_SETTLE:
            // 잔류 버퍼 drain 후 안정화 시간(500ms) 경과 시 LISTEN 모드로 복귀
            if (elapsed >= LD2420_STARTUP_SETTLE_MS) setState(LD2420State::LISTEN);
            break;

        case LD2420State::LISTEN:
            // 첫 번째 유효 패킷 또는 텍스트 라인 수신 시 RUN 모드로 정상 진입
            if (_rxPos > 0 || _linePos > 0) {
                xEventGroupSetBits(_events, EV_STARTUP_DONE);
                emitEvent("startup_done");
                setState(LD2420State::RUN);
            } else if (elapsed >= LD2420_STARTUP_TIMEOUT_MS) {
                // 10초 동안 데이터가 없어도 ESPHome처럼 포기(give-up) 후 RUN으로 강제 진입 (파싱은 계속 유지)
                emitEvent("startup_give_up");
                setState(LD2420State::RUN);
            }
            break;

        case LD2420State::CONFIG:
            // 설정 모드 플래그가 해제되면 정상 감지 상태(RUN)로 복귀
            if (!(xEventGroupGetBits(_events) & EV_CONFIG_MODE)) setState(LD2420State::RUN);
            break;

        case LD2420State::CALIBRATE:
            // 캘리브레이션 64개 샘플 수집 완료 시 이벤트 통지
            if (_cal.state() == LD2420CalState::READY) {
                xEventGroupSetBits(_events, EV_CAL_READY);
                emitEvent("cal_ready");
            }
            break;

        default:
            break;
    }
}

// =======================================================
// [바이트 수신 및 프레임 구분 파서]
// =======================================================

/**
 * @brief 수신된 1바이트를 분석하여 버퍼링 및 프레임 완성을 감지
 * @param b 수신된 바이트
 */
void LD2420::handleByte(uint8_t b) {
    // 바이너리 수신 버퍼 누적
    if (_rxPos < sizeof(_rxBuf)) _rxBuf[_rxPos++] = b;

    // 1. 명령 응답(ACK) 프레임 완성 검사 (푸터: 04 03 02 01)
    if (_rxPos >= 4 && _rxBuf[_rxPos - 4] == 0x04 && _rxBuf[_rxPos - 3] == 0x03 && _rxBuf[_rxPos - 2] == 0x02 &&
        _rxBuf[_rxPos - 1] == 0x01) {
        if (LD2420Protocol::isCommandFrame(_rxBuf, _rxPos)) {
            handleCommandFrame(_rxBuf, _rxPos);
        }
        _rxPos = 0; // 버퍼 리셋
    }
    // 2. 에너지 리포트 프레임 완성 검사 (푸터: F8 F7 F6 F5)
    else if (_rxPos >= 4 && _rxBuf[_rxPos - 4] == 0xF8 && _rxBuf[_rxPos - 3] == 0xF7 && _rxBuf[_rxPos - 2] == 0xF6 &&
             _rxBuf[_rxPos - 1] == 0xF5) {
        if (LD2420Protocol::isEnergyFrame(_rxBuf, _rxPos)) {
            handleEnergyFrame(_rxBuf, _rxPos);
        }
        _rxPos = 0; // 버퍼 리셋
    }
    // 3. Simple 모드 텍스트 라인 검사 (개행 문자 수신 시 라인 파싱)
    else if (b == '\n' || b == '\r') {
        if (_linePos > 0) {
            _lineBuf[_linePos] = '\0';
            handleSimpleLine(_lineBuf, _linePos);
            _linePos = 0;
        }
    }
    // 4. 바이너리 헤더가 아닌 일반 ASCII 문자일 경우 라인 버퍼에 누적
    else if (_rxPos == 1 && !LD2420Protocol::isCommandFrame(_rxBuf, 1) && !LD2420Protocol::isEnergyFrame(_rxBuf, 1)) {
        if (b >= 0x20 && b < 0x7F && _linePos < sizeof(_lineBuf) - 1) {
            _lineBuf[_linePos++] = (char)b;
        }
        _rxPos = 0;
    }

    // 버퍼 오버플로우 방지 리셋
    if (_rxPos >= sizeof(_rxBuf)) _rxPos = 0;
}

// =======================================================
// [프레임 디코딩 핸들러]
// =======================================================

/**
 * @brief 명령 ACK 패킷 수신 처리
 * cmdTask의 재시도 타이머를 해제하고, 버전 정보 또는 설정 모드 상태를 갱신합니다.
 */
void LD2420::handleCommandFrame(const uint8_t* buf, size_t len) {
    uint16_t cmd = 0, err = 0;
    if (!LD2420Protocol::parseAck(buf, len, &cmd, &err)) return;

    // ACK가 수신되었으므로 대기 중이던 cmdTask의 상태를 해제
    if (_cmdState == LD2420CmdState::WAIT_ACK || _cmdState == LD2420CmdState::RETRY) {
        _cmdState = LD2420CmdState::NONE;
    }

    // 펌웨어 버전 응답 수신 시 이벤트 통지
    if (cmd == LD2420_CMD_READ_VERSION) {
        LD2420VersionInfo vi;
        if (LD2420Protocol::parseVersion(buf, len, &vi)) {
            emitEvent(vi.firmware);
        }
    }

    // 설정 모드 진입/종료 성공 여부 반영
    if (cmd == LD2420_CMD_ENABLE_CONF && err == 0) {
        xEventGroupSetBits(_events, EV_CONFIG_MODE);
    } else if (cmd == LD2420_CMD_DISABLE_CONF && err == 0) {
        xEventGroupClearBits(_events, EV_CONFIG_MODE);
    }
}

/**
 * @brief 16개 게이트 실시간 에너지 프레임 수신 처리
 * 최신 감지 데이터를 갱신하고, 캘리브레이션 모드일 경우 샘플링 피드를 수행합니다.
 */
void LD2420::handleEnergyFrame(const uint8_t* buf, size_t len) {
    LD2420TargetData d;
    if (!LD2420Protocol::parseEnergyFrame(buf, len, &d)) return;

    d.sequence = _latest.sequence + 1;
    publishData(d);

    // 자동 캘리브레이션 수집 중일 경우 샘플 주입
    if (_cal.isCollecting()) {
        _cal.feed(d, millis());
        if (_cal.state() == LD2420CalState::READY) {
            xEventGroupSetBits(_events, EV_CAL_READY);
        }
    }
}

/**
 * @brief Simple 텍스트 모드("ON Range:xxx", "OFF") 라인 처리
 */
void LD2420::handleSimpleLine(const char* line, size_t len) {
    LD2420TargetData d = _latest;
    if (LD2420Protocol::parseSimpleLine(line, len, &d)) {
        d.sequence = _latest.sequence + 1;
        publishData(d);
    }
}

// =======================================================
// [데이터 퍼블리시 및 콜백 전달]
// =======================================================

/**
 * @brief 파싱된 최신 타겟 데이터를 뮤텍스 보호 하에 내부 저장소에 반영하고 사용자 콜백을 실행
 * @param d 갱신된 타겟 데이터
 */
void LD2420::publishData(const LD2420TargetData& d) {
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _latest = d;
        xSemaphoreGive(_dataMutex);
    }
    // 등록된 사용자 데이터 콜백 실행
    if (_dataCb) _dataCb(d, _dataCtx);
}

/**
 * @brief 내부 시스템 이벤트를 등록된 사용자 이벤트 콜백에 전달
 * @param evt 이벤트 문자열
 */
void LD2420::emitEvent(const char* evt) {
    if (_eventCb) _eventCb(evt, _eventCtx);
}

/**
 * @brief 스레드 세이프하게 최신 타겟 데이터 스냅샷을 복사하여 반환
 * @return LD2420TargetData 구조체 복사본
 */
LD2420TargetData LD2420::getLatestData() {
    LD2420TargetData copy;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = _latest;
        xSemaphoreGive(_dataMutex);
    } else {
        memset(&copy, 0, sizeof(copy));
    }
    return copy;
}

// =======================================================
// [CMD 비동기 송신 태스크 메인 루프]
// =======================================================

/**
 * @brief 명령 큐 감시 및 송신, 타임아웃 감시 및 재전송 처리 태스크 (5ms 주기 실행)
 * - 유휴 상태일 때 _cmdQueue에서 명령 프레임을 꺼내 UART로 전송
 * - 1초(LD2420_CMD_ACK_TIMEOUT_MS) 내 ACK 미수신 시 최대 3회까지 자동 재전송
 */
void LD2420::cmdTaskLoop() {
    const TickType_t ackTick = pdMS_TO_TICKS(LD2420_CMD_ACK_TIMEOUT_MS);

    while (true) {
        if (xEventGroupGetBits(_events) & EV_SHUTDOWN) {
            vTaskDelete(nullptr);
            return;
        }

        // 1. ACK 타임아웃 감시 및 재전송 로직
        if (_cmdState == LD2420CmdState::WAIT_ACK || _cmdState == LD2420CmdState::RETRY) {
            if (millis() - _cmdSentMs > LD2420_CMD_ACK_TIMEOUT_MS) {
                _retryCount++;
                if (_retryCount < LD2420_CMD_MAX_RETRIES) {
                    uint8_t frame[32];
                    size_t  n = 0;
                    // 재전송을 위한 바이너리 프레임 재생성
                    switch (_currentCmd.command) {
                        case LD2420_CMD_ENABLE_CONF:
                            n = LD2420Protocol::buildEnableConfig(frame);
                            break;
                        case LD2420_CMD_DISABLE_CONF:
                            n = LD2420Protocol::buildDisableConfig(frame);
                            break;
                        case LD2420_CMD_WRITE_SYS_PARAM:
                            n = LD2420Protocol::buildSysParam(
                                frame,
                                _currentCmd.data[2],
                                _currentCmd.data[3],
                                _currentCmd.data[4] | (_currentCmd.data[5] << 8),
                                (LD2420Mode)(_currentCmd.data[0] | (_currentCmd.data[1] << 8)));
                            break;
                        default:
                            _cmdState = LD2420CmdState::NONE;
                            break;
                    }
                    if (n > 0) {
                        sendFrameRaw(frame, n);
                        _cmdSentMs = millis();
                        _cmdState  = LD2420CmdState::WAIT_ACK;
                    }
                } else {
                    // 최대 재시도 초과: 타임아웃 이벤트 발행 및 명령 상태 해제
                    emitEvent("cmd_timeout");
                    _cmdState = LD2420CmdState::NONE;
                }
            }
        }

        // 2. 유휴(NONE) 상태일 때 큐에서 다음 명령 인출 및 송신
        if (_cmdState == LD2420CmdState::NONE) {
            LD2420CmdFrame frame;
            if (xQueueReceive(_cmdQueue, &frame, pdMS_TO_TICKS(20)) == pdTRUE) {
                _currentCmd = frame;
                _retryCount = 0;
                _cmdSentMs  = millis();

                uint8_t buf[32];
                size_t  n = 0;
                switch (frame.command) {
                    case LD2420_CMD_ENABLE_CONF:
                        n = LD2420Protocol::buildEnableConfig(buf);
                        break;
                    case LD2420_CMD_DISABLE_CONF:
                        n = LD2420Protocol::buildDisableConfig(buf);
                        break;
                    case LD2420_CMD_READ_VERSION:
                        n = LD2420Protocol::buildVersionRead(buf);
                        break;
                    case LD2420_CMD_RESTART:
                        n = LD2420Protocol::buildRestart(buf);
                        break;
                    case LD2420_CMD_WRITE_SYS_PARAM:
                        n = LD2420Protocol::buildSysParam(buf,
                                                          frame.data[2],
                                                          frame.data[3],
                                                          frame.data[4] | (frame.data[5] << 8),
                                                          (LD2420Mode)(frame.data[0] | (frame.data[1] << 8)));
                        break;
                    case LD2420_CMD_WRITE_GATE_PARAM:
                        n = LD2420Protocol::buildGateParam(buf,
                                                           frame.data[0],
                                                           frame.data[2] | (frame.data[3] << 8),
                                                           frame.data[8] | (frame.data[9] << 8));
                        break;
                }

                if (n > 0) {
                    sendFrameRaw(buf, n);
                    _cmdState = LD2420CmdState::WAIT_ACK;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**
 * @brief 하드웨어 시리얼 포트로 원시 바이트 스트림 송신
 * @param data 데이터 포인터
 * @param len 전송할 바이트 수
 * @return 전송 성공 여부
 */
bool LD2420::sendFrameRaw(const uint8_t* data, size_t len) {
    return _serial.write(data, len) == len;
}

/**
 * @brief 비동기 명령 큐에 명령 프레임 삽입
 * @param frame 전송할 명령 구조체
 * @param timeout 큐 인큐 대기 시간 (기본 50ms)
 * @return 성공 여부
 */
bool LD2420::enqueueCommand(const LD2420CmdFrame& frame, TickType_t timeout) {
    if (!_cmdQueue) return false;
    return xQueueSend(_cmdQueue, &frame, timeout) == pdTRUE;
}

// =======================================================
// [공개 API — 비동기 설정 명령]
// 모든 함수는 큐에 명령을 넣고 즉시 반환(non-blocking)됩니다.
// =======================================================

/**
 * @brief 특정 게이트의 감도 임계값 변경 요청
 * @param gate 대상 게이트 (0 ~ 15)
 * @param move 움직임 감도 임계값 (0 ~ 100)
 * @param still 정지 감도 임계값 (0 ~ 100)
 */
bool LD2420::requestGateThreshold(uint8_t gate, uint16_t move, uint16_t still) {
    if (gate >= LD2420_MAX_GATES) return false;
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_WRITE_GATE_PARAM;
    f.data[0]        = gate;
    f.data[1]        = 0;
    f.data[2]        = move & 0xFF;
    f.data[3]        = move >> 8;
    f.data[4]        = 0;
    f.data[5]        = 0;
    f.data[6]        = still & 0xFF;
    f.data[7]        = still >> 8;
    f.data[8]        = still & 0xFF;
    f.data[9]        = still >> 8; // cmdTask 빌더 재사용
    f.data_length    = 10;
    return enqueueCommand(f);
}

/**
 * @brief 16개 전체 게이트의 감도 임계값을 순차적으로 변경 요청
 * @param cfg 16개 게이트의 설정이 담긴 구조체
 */
bool LD2420::requestAllGateThresholds(const LD2420GateConfig& cfg) {
    for (uint8_t g = 0; g < LD2420_MAX_GATES; g++) {
        if (!requestGateThreshold(g, cfg.move_threshold[g], cfg.still_threshold[g])) return false;
    }
    return true;
}

/**
 * @brief 최소 및 최대 감지 거리 게이트 범위 제한 요청
 * @param minGate 최소 게이트 (0 ~ 15)
 * @param maxGate 최대 게이트 (0 ~ 15)
 */
bool LD2420::requestMinMaxDistance(uint8_t minGate, uint8_t maxGate) {
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_WRITE_SYS_PARAM;
    f.data[2]        = minGate;
    f.data[3]        = maxGate;
    f.data_length    = 8;
    return enqueueCommand(f);
}

/**
 * @brief 인체 부재(미감지) 판정 유지 타임아웃(초) 설정 요청
 * @param seconds 유지 시간 (단위: 초)
 */
bool LD2420::requestTimeout(uint16_t seconds) {
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_WRITE_SYS_PARAM;
    f.data[4]        = seconds & 0xFF;
    f.data[5]        = seconds >> 8;
    f.data_length    = 8;
    return enqueueCommand(f);
}

/**
 * @brief 센서 출력 모드(SIMPLE, ENERGY 등) 변경 요청
 * @param mode LD2420Mode 열거형 값
 */
bool LD2420::requestMode(LD2420Mode mode) {
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_WRITE_SYS_PARAM;
    f.data[0]        = (uint8_t)mode;
    f.data[1]        = (uint8_t)((uint16_t)mode >> 8);
    f.data_length    = 8;
    return enqueueCommand(f);
}

/**
 * @brief 센서 펌웨어 버전 정보 조회 요청
 */
bool LD2420::requestFirmwareVersion() {
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_READ_VERSION;
    f.data_length    = 0;
    return enqueueCommand(f);
}

/**
 * @brief 센서 모듈 재시작(리셋) 요청
 */
bool LD2420::requestRestart() {
    LD2420CmdFrame f = {};
    f.command        = LD2420_CMD_RESTART;
    f.data_length    = 0;
    return enqueueCommand(f);
}

/**
 * @brief 센서 설정 모드 진입(true) 또는 종료(false) 요청
 * @param enable true: 설정 모드 진입, false: 종료 후 감지 모드 복귀
 */
bool LD2420::requestConfigMode(bool enable) {
    LD2420CmdFrame f = {};
    f.command        = enable ? LD2420_CMD_ENABLE_CONF : LD2420_CMD_DISABLE_CONF;
    f.data_length    = 0;
    if (enable) setState(LD2420State::CONFIG);
    return enqueueCommand(f);
}

// =======================================================
// [공개 API — 감도 프리셋]
// =======================================================

/**
 * @brief 기본 권장 감도 프리셋 적용
 * - 0~5번 게이트: 감도 10 (민감 감지)
 * - 6~15번 게이트: 감도 100 (원거리 오탐 차단)
 */
bool LD2420::requestDefaultThresholds() {
    LD2420GateConfig c;
    c.setDefault();
    return requestAllGateThresholds(c);
}

/**
 * @brief 초고감도 프리셋 적용
 * 모든 게이트(0~15)의 임계값을 5로 설정하여 미세 움직임 포착
 */
bool LD2420::requestSensitiveThresholds() {
    LD2420GateConfig c;
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        c.move_threshold[g]  = 5;
        c.still_threshold[g] = 5;
    }
    return requestAllGateThresholds(c);
}

/**
 * @brief 균형(Balanced) 감도 프리셋 적용
 * - 0~7번 게이트: 감도 15
 * - 8~15번 게이트: 감도 50
 */
bool LD2420::requestBalancedThresholds() {
    LD2420GateConfig c;
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        c.move_threshold[g]  = (g <= 7) ? 15 : 50;
        c.still_threshold[g] = (g <= 7) ? 15 : 50;
    }
    return requestAllGateThresholds(c);
}

// =======================================================
// [공개 API — 자동 캘리브레이션]
// =======================================================

/**
 * @brief 자동 캘리브레이션 시작
 * 센서를 ENERGY 모드로 변경하고 5초 주기로 배경 노이즈 수집을 시작합니다.
 */
bool LD2420::startCalibration() {
    if (_cal.isCollecting()) return false;
    _cal.start();
    setState(LD2420State::CALIBRATE);
    requestMode(LD2420Mode::ENERGY);
    emitEvent("cal_start");
    return true;
}

/**
 * @brief 캘리브레이션 결과로 연산된 최적 임계값을 센서에 기록하고 SIMPLE 모드로 복귀
 */
bool LD2420::applyCalibration() {
    if (!_cal.isReady()) return false;
    LD2420GateConfig c  = _cal.computeConfig();
    bool             ok = requestAllGateThresholds(c);
    if (ok) {
        _cal.reset();
        requestMode(LD2420Mode::SIMPLE);
        setState(LD2420State::RUN);
        emitEvent("cal_applied");
    }
    return ok;
}

/**
 * @brief 진행 중인 캘리브레이션을 중단하고 정상 RUN 모드로 복귀
 */
void LD2420::cancelCalibration() {
    _cal.cancel();
    setState(LD2420State::RUN);
    emitEvent("cal_cancel");
}

// =======================================================
// [공개 API — 콜백 등록]
// =======================================================

/**
 * @brief 타겟 감지 데이터 콜백 등록
 * @param cb 콜백 함수 포인터
 * @param ctx 콜백 호출 시 전달될 사용자 포인터
 */
void LD2420::onData(LD2420DataCallback cb, void* ctx) {
    _dataCb  = cb;
    _dataCtx = ctx;
}

/**
 * @brief 시스템 이벤트 콜백 등록
 * @param cb 콜백 함수 포인터
 * @param ctx 콜백 호출 시 전달될 사용자 포인터
 */
void LD2420::onEvent(LD2420EventCallback cb, void* ctx) {
    _eventCb  = cb;
    _eventCtx = ctx;
}

// =======================================================
// [거리 및 게이트 환산 유틸리티]
// =======================================================

/**
 * @brief 미터 단위 거리를 게이트 번호(0~15)로 환산
 * @param m 거리 (단위: 미터)
 * @return 게이트 번호 (0 ~ 15 사이로 클램핑)
 */
uint8_t LD2420::metersToGate(float m) {
    int g = (int)(m / LD2420_GATE_DISTANCE_M);
    if (g < 0) return 0;
    if (g >= LD2420_MAX_GATES) return LD2420_MAX_GATES - 1;
    return (uint8_t)g;
}
