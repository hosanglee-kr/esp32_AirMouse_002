// =======================================================
// File: src/LD2420_001.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420_001.cpp
 * 모듈약어 : 
 * 모듈명 : 
 * ------------------------------------------------------
 * 기능 요약
 *  - 
 *  - 
 *  - 
 *
 * [설계]
 *  - 
 *  - .
 *  - 
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 
 * ------------------------------------------------------
 */
 
 #include "LD2420_001.h"

// ==================== 생성자 / 소멸자 ====================
LD2420::LD2420(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud)
  : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud) {
  memset(&_latest, 0, sizeof(_latest));
}

LD2420::~LD2420() { end(); }

// ==================== 초기화 ====================
bool LD2420::begin(uint32_t rxStack, uint32_t cmdStack,
                   UBaseType_t rxPrio, UBaseType_t cmdPrio) {
  _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  delay(50);
  while (_serial.available()) _serial.read();

  _dataMutex = xSemaphoreCreateMutex();
  _cmdQueue  = xQueueCreate(LD2420_CMD_QUEUE_SIZE, sizeof(LD2420CmdFrame));
  _events    = xEventGroupCreate();
  if (!_dataMutex || !_cmdQueue || !_events) return false;

  setState(LD2420State::STARTUP);

  // rxTask: 코어 0에 고정 (loopTask/Wi-Fi와 분리)
  if (xTaskCreatePinnedToCore(rxTaskTrampoline, "ld2420_rx",
                              rxStack, this, rxPrio,
                              &_rxTask, 0) != pdPASS) return false;

  // cmdTask: 코어 0에 고정, rxTask보다 낮은 우선순위
  if (xTaskCreatePinnedToCore(cmdTaskTrampoline, "ld2420_cmd",
                              cmdStack, this, cmdPrio,
                              &_cmdTask, 0) != pdPASS) {
    vTaskDelete(_rxTask);
    _rxTask = nullptr;
    return false;
  }

  // 시작 시퀀스: 펌웨어 버전 요청
  requestFirmwareVersion();
  return true;
}

void LD2420::end() {
  if (_events) xEventGroupSetBits(_events, EV_SHUTDOWN);
  vTaskDelay(pdMS_TO_TICKS(20));

  if (_rxTask)  { vTaskDelete(_rxTask);  _rxTask  = nullptr; }
  if (_cmdTask) { vTaskDelete(_cmdTask); _cmdTask = nullptr; }

  if (_dataMutex) { vSemaphoreDelete(_dataMutex); _dataMutex = nullptr; }
  if (_cmdQueue)  { vQueueDelete(_cmdQueue);     _cmdQueue  = nullptr; }
  if (_events)    { vEventGroupDelete(_events);  _events    = nullptr; }
}

// ==================== 태스크 트램폴린 ====================
void LD2420::rxTaskTrampoline(void *arg)  { static_cast<LD2420*>(arg)->rxTaskLoop(); }
void LD2420::cmdTaskTrampoline(void *arg) { static_cast<LD2420*>(arg)->cmdTaskLoop(); }

// ==================== RX 태스크 ====================
void LD2420::rxTaskLoop() {
  const TickType_t period = pdMS_TO_TICKS(5);

  while (true) {
    // 셧다운 이벤트 확인
    if (xEventGroupGetBits(_events) & EV_SHUTDOWN) {
      vTaskDelete(nullptr);
      return;
    }

    // 배치 수신 (ESPHome read_batch_와 동일 전략)
    size_t avail = _serial.available();
    if (avail > 0) {
      uint8_t chunk[64];
      while (avail > 0) {
        size_t toRead = (avail > sizeof(chunk)) ? sizeof(chunk) : avail;
        size_t n = _serial.readBytes(chunk, toRead);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) handleByte(chunk[i]);
        avail -= n;
      }
    }

    processStateMachine(millis());
    vTaskDelay(period);
  }
}

// ==================== 상태 머신 ====================
void LD2420::processStateMachine(uint32_t now) {
  const uint32_t elapsed = now - _stateStartMs;

  switch (_state) {
    case LD2420State::STARTUP:
      if (elapsed >= LD2420_STARTUP_SETTLE_MS)
        setState(LD2420State::LISTEN);
      break;

    case LD2420State::LISTEN_SETTLE:
      // (RX 배치 drain 후 전환)
      if (elapsed >= LD2420_STARTUP_SETTLE_MS)
        setState(LD2420State::LISTEN);
      break;

    case LD2420State::LISTEN:
      // 첫 데이터 수신 시 RUN 진입
      if (_rxPos > 0 || _linePos > 0) {
        xEventGroupSetBits(_events, EV_STARTUP_DONE);
        emitEvent("startup_done");
        setState(LD2420State::RUN);
      } else if (elapsed >= LD2420_STARTUP_TIMEOUT_MS) {
        // ESPHome의 give-up 경로와 동일: 파싱은 계속
        emitEvent("startup_give_up");
        setState(LD2420State::RUN);
      }
      break;

    case LD2420State::CONFIG:
      if (!(xEventGroupGetBits(_events) & EV_CONFIG_MODE))
        setState(LD2420State::RUN);
      break;

    case LD2420State::CALIBRATE:
      if (_cal.state() == LD2420CalState::READY) {
        xEventGroupSetBits(_events, EV_CAL_READY);
        emitEvent("cal_ready");
      }
      break;

    default:
      break;
  }
}

// ==================== 바이트 파서 ====================
void LD2420::handleByte(uint8_t b) {
  // 라인/프레임 버퍼에 누적
  if (_rxPos < sizeof(_rxBuf)) _rxBuf[_rxPos++] = b;

  // 명령 프레임 완성 검사 (Footer 04 03 02 01)
  if (_rxPos >= 4 &&
      _rxBuf[_rxPos-4] == 0x04 && _rxBuf[_rxPos-3] == 0x03 &&
      _rxBuf[_rxPos-2] == 0x02 && _rxBuf[_rxPos-1] == 0x01) {
    if (LD2420Protocol::isCommandFrame(_rxBuf, _rxPos))
      handleCommandFrame(_rxBuf, _rxPos);
    _rxPos = 0;
  }
  // 에너지 프레임 완성 검사 (Footer F8 F7 F6 F5)
  else if (_rxPos >= 4 &&
           _rxBuf[_rxPos-4] == 0xF8 && _rxBuf[_rxPos-3] == 0xF7 &&
           _rxBuf[_rxPos-2] == 0xF6 && _rxBuf[_rxPos-1] == 0xF5) {
    if (LD2420Protocol::isEnergyFrame(_rxBuf, _rxPos))
      handleEnergyFrame(_rxBuf, _rxPos);
    _rxPos = 0;
  }
  // 텍스트 라인 (Simple 모드)
  else if (b == '\n' || b == '\r') {
    if (_linePos > 0) {
      _lineBuf[_linePos] = '\0';
      handleSimpleLine(_lineBuf, _linePos);
      _linePos = 0;
    }
  } else if (_rxPos == 1 && !LD2420Protocol::isCommandFrame(_rxBuf, 1) &&
             !LD2420Protocol::isEnergyFrame(_rxBuf, 1)) {
    // 바이너리 프레임이 아닌 경우 텍스트로 처리
    if (b >= 0x20 && b < 0x7F && _linePos < sizeof(_lineBuf) - 1) {
      _lineBuf[_linePos++] = (char)b;
    }
    _rxPos = 0;
  }

  // 오버플로우 보호
  if (_rxPos >= sizeof(_rxBuf)) _rxPos = 0;
}

// ==================== 프레임 핸들러 ====================
void LD2420::handleCommandFrame(const uint8_t *buf, size_t len) {
  uint16_t cmd = 0, err = 0;
  if (!LD2420Protocol::parseAck(buf, len, &cmd, &err)) return;

  // ACK 수신 → cmdTask의 재시도 상태 해제
  if (_cmdState == LD2420CmdState::WAIT_ACK || _cmdState == LD2420CmdState::RETRY) {
    _cmdState = LD2420CmdState::NONE;
  }

  // 펌웨어 버전 응답
  if (cmd == LD2420_CMD_READ_VERSION) {
    LD2420VersionInfo vi;
    if (LD2420Protocol::parseVersion(buf, len, &vi)) {
      emitEvent(vi.firmware);
    }
  }

  // 설정 모드 진입/종료 ACK 처리
  if (cmd == LD2420_CMD_ENABLE_CONF && err == 0) {
    xEventGroupSetBits(_events, EV_CONFIG_MODE);
  } else if (cmd == LD2420_CMD_DISABLE_CONF && err == 0) {
    xEventGroupClearBits(_events, EV_CONFIG_MODE);
  }
}

void LD2420::handleEnergyFrame(const uint8_t *buf, size_t len) {
  LD2420TargetData d;
  if (!LD2420Protocol::parseEnergyFrame(buf, len, &d)) return;

  d.sequence = _latest.sequence + 1;
  publishData(d);

  if (_cal.isCollecting()) {
    _cal.feed(d, millis());
    if (_cal.state() == LD2420CalState::READY)
      xEventGroupSetBits(_events, EV_CAL_READY);
  }
}

void LD2420::handleSimpleLine(const char *line, size_t len) {
  LD2420TargetData d = _latest;
  if (LD2420Protocol::parseSimpleLine(line, len, &d)) {
    d.sequence = _latest.sequence + 1;
    publishData(d);
  }
}

// ==================== 데이터 퍼블리시 ====================
void LD2420::publishData(const LD2420TargetData &d) {
  if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    _latest = d;
    xSemaphoreGive(_dataMutex);
  }
  if (_dataCb) _dataCb(d, _dataCtx);
}

void LD2420::emitEvent(const char *evt) {
  if (_eventCb) _eventCb(evt, _eventCtx);
}

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

// ==================== CMD 태스크 ====================
void LD2420::cmdTaskLoop() {
  const TickType_t ackTick = pdMS_TO_TICKS(LD2420_CMD_ACK_TIMEOUT_MS);

  while (true) {
    if (xEventGroupGetBits(_events) & EV_SHUTDOWN) {
      vTaskDelete(nullptr);
      return;
    }

    // ACK 타임아웃 감시
    if (_cmdState == LD2420CmdState::WAIT_ACK ||
        _cmdState == LD2420CmdState::RETRY) {
      if (millis() - _cmdSentMs > LD2420_CMD_ACK_TIMEOUT_MS) {
        _retryCount++;
        if (_retryCount < LD2420_CMD_MAX_RETRIES) {
          uint8_t frame[32];
          size_t n = 0;
          // 재전송: 프레임 재빌드 (build helper 재호출)
          switch (_currentCmd.command) {
            case LD2420_CMD_ENABLE_CONF:
              n = LD2420Protocol::buildEnableConfig(frame); break;
            case LD2420_CMD_DISABLE_CONF:
              n = LD2420Protocol::buildDisableConfig(frame); break;
            case LD2420_CMD_WRITE_SYS_PARAM:
              n = LD2420Protocol::buildSysParam(
                frame, _currentCmd.data[2], _currentCmd.data[3],
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
            _cmdState = LD2420CmdState::WAIT_ACK;
          }
        } else {
          emitEvent("cmd_timeout");
          _cmdState = LD2420CmdState::NONE;
        }
      }
    }

    // 큐에서 다음 명령 꺼내기
    if (_cmdState == LD2420CmdState::NONE) {
      LD2420CmdFrame frame;
      if (xQueueReceive(_cmdQueue, &frame, pdMS_TO_TICKS(20)) == pdTRUE) {
        _currentCmd = frame;
        _retryCount = 0;
        _cmdSentMs  = millis();

        uint8_t buf[32];
        size_t n = 0;
        switch (frame.command) {
          case LD2420_CMD_ENABLE_CONF:
            n = LD2420Protocol::buildEnableConfig(buf); break;
          case LD2420_CMD_DISABLE_CONF:
            n = LD2420Protocol::buildDisableConfig(buf); break;
          case LD2420_CMD_READ_VERSION:
            n = LD2420Protocol::buildVersionRead(buf); break;
          case LD2420_CMD_RESTART:
            n = LD2420Protocol::buildRestart(buf); break;
          case LD2420_CMD_WRITE_SYS_PARAM:
            n = LD2420Protocol::buildSysParam(
              buf, frame.data[2], frame.data[3],
              frame.data[4] | (frame.data[5] << 8),
              (LD2420Mode)(frame.data[0] | (frame.data[1] << 8)));
            break;
          case LD2420_CMD_WRITE_GATE_PARAM:
            n = LD2420Protocol::buildGateParam(
              buf, frame.data[0],
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

bool LD2420::sendFrameRaw(const uint8_t *data, size_t len) {
  return _serial.write(data, len) == len;
}

// ==================== 명령 큐 ====================
bool LD2420::enqueueCommand(const LD2420CmdFrame &frame, TickType_t timeout) {
  if (!_cmdQueue) return false;
  return xQueueSend(_cmdQueue, &frame, timeout) == pdTRUE;
}

// ==================== 공개 API — 설정 명령 ====================
bool LD2420::requestGateThreshold(uint8_t gate, uint16_t move, uint16_t still) {
  if (gate >= LD2420_MAX_GATES) return false;
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_WRITE_GATE_PARAM;
  f.data[0] = gate;
  f.data[1] = 0;
  f.data[2] = move & 0xFF;  f.data[3] = move >> 8;
  f.data[4] = 0; f.data[5] = 0;
  f.data[6] = still & 0xFF; f.data[7] = still >> 8;
  f.data[8] = still & 0xFF; f.data[9] = still >> 8;  // cmdTask에서 재사용
  f.data_length = 10;
  return enqueueCommand(f);
}

bool LD2420::requestAllGateThresholds(const LD2420GateConfig &cfg) {
  for (uint8_t g = 0; g < LD2420_MAX_GATES; g++) {
    if (!requestGateThreshold(g, cfg.move_threshold[g], cfg.still_threshold[g]))
      return false;
  }
  return true;
}

bool LD2420::requestMinMaxDistance(uint8_t minGate, uint8_t maxGate) {
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_WRITE_SYS_PARAM;
  f.data[2] = minGate;
  f.data[3] = maxGate;
  f.data_length = 8;
  return enqueueCommand(f);
}

bool LD2420::requestTimeout(uint16_t seconds) {
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_WRITE_SYS_PARAM;
  f.data[4] = seconds & 0xFF;
  f.data[5] = seconds >> 8;
  f.data_length = 8;
  return enqueueCommand(f);
}

bool LD2420::requestMode(LD2420Mode mode) {
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_WRITE_SYS_PARAM;
  f.data[0] = (uint8_t)mode;
  f.data[1] = (uint8_t)((uint16_t)mode >> 8);
  f.data_length = 8;
  return enqueueCommand(f);
}

bool LD2420::requestFirmwareVersion() {
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_READ_VERSION;
  f.data_length = 0;
  return enqueueCommand(f);
}

bool LD2420::requestRestart() {
  LD2420CmdFrame f = {};
  f.command = LD2420_CMD_RESTART;
  f.data_length = 0;
  return enqueueCommand(f);
}

bool LD2420::requestConfigMode(bool enable) {
  LD2420CmdFrame f = {};
  f.command = enable ? LD2420_CMD_ENABLE_CONF : LD2420_CMD_DISABLE_CONF;
  f.data_length = 0;
  if (enable) setState(LD2420State::CONFIG);
  return enqueueCommand(f);
}

// ==================== 프리셋 ====================
bool LD2420::requestDefaultThresholds() {
  LD2420GateConfig c; c.setDefault();
  return requestAllGateThresholds(c);
}

bool LD2420::requestSensitiveThresholds() {
  LD2420GateConfig c;
  for (int g = 0; g < LD2420_MAX_GATES; g++) {
    c.move_threshold[g] = 5; c.still_threshold[g] = 5;
  }
  return requestAllGateThresholds(c);
}

bool LD2420::requestBalancedThresholds() {
  LD2420GateConfig c;
  for (int g = 0; g < LD2420_MAX_GATES; g++) {
    c.move_threshold[g]  = (g <= 7) ? 15 : 50;
    c.still_threshold[g] = (g <= 7) ? 15 : 50;
  }
  return requestAllGateThresholds(c);
}

// ==================== 캘리브레이션 ====================
bool LD2420::startCalibration() {
  if (_cal.isCollecting()) return false;
  _cal.start();
  setState(LD2420State::CALIBRATE);
  requestMode(LD2420Mode::ENERGY);
  emitEvent("cal_start");
  return true;
}

bool LD2420::applyCalibration() {
  if (!_cal.isReady()) return false;
  LD2420GateConfig c = _cal.computeConfig();
  bool ok = requestAllGateThresholds(c);
  if (ok) {
    _cal.reset();
    requestMode(LD2420Mode::SIMPLE);
    setState(LD2420State::RUN);
    emitEvent("cal_applied");
  }
  return ok;
}

void LD2420::cancelCalibration() {
  _cal.cancel();
  setState(LD2420State::RUN);
  emitEvent("cal_cancel");
}

// ==================== 콜백 ====================
void LD2420::onData(LD2420DataCallback cb, void *ctx) {
  _dataCb = cb; _dataCtx = ctx;
}
void LD2420::onEvent(LD2420EventCallback cb, void *ctx) {
  _eventCb = cb; _eventCtx = ctx;
}

// ==================== 유틸 ====================
uint8_t LD2420::metersToGate(float m) {
  int g = (int)(m / LD2420_GATE_DISTANCE_M);
  if (g < 0) return 0;
  if (g >= LD2420_MAX_GATES) return LD2420_MAX_GATES - 1;
  return (uint8_t)g;
}
