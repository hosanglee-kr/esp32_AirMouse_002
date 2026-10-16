// =======================================================
// File: src/LD2420_Proto_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Proto_001.h
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 프로토콜 프레임 빌더 및 파서 선언부
 * ------------------------------------------------------
 * 기능 요약
 *  - LD2420 제어 명령 바이너리 프레임 생성 (순수 함수 기반 빌더)
 *  - 센서 응답(ACK, 버전), 바이너리 에너지 프레임, Simple 모드 텍스트 라인 파싱
 *  - 프레임 헤더 검사 및 가변 프레임 전체 길이 판별 유틸리티 제공
 *
 * [설계]
 *  - ESPHome의 build helper 및 파서 로직과 100% 동일한 바이너리 규격 적용
 *  - 내부 상태를 갖지 않는 정적 클래스(Static Utility Class)로 설계하여
 *    어느 컨텍스트에서든 재진입(Reentrant) 및 스레드 세이프하게 호출 가능
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 클래스명            : LD2420 접두사 + PascalCase (LD2420Protocol)
 *  - 함수명              : camelCase (build*, parse*, is*)
 *  - 매개변수            : camelCase (out, frame, len 등)
 * ------------------------------------------------------
 */

#include "LD2420_Types_001.h"
#include <Stream.h>

/**
 * @brief LD2420 센서 시리얼 통신 프로토콜 인코딩/디코딩 정적 클래스
 */
class LD2420Protocol {
  public:
    // =======================================================
    // [프레임 빌더 (Frame Builders)]
    // 외부 버퍼(out)에 프레임을 쓰고 총 바이트 길이를 반환합니다.
    // =======================================================

    /**
     * @brief 설정 모드 진입(Enable Config) 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 12바이트 이상)
     * @return 기록된 총 바이트 수 (12바이트: Header 4 + Len 2 + CMD 2 + Ver 2 + Footer 4)
     */
    static size_t buildEnableConfig(uint8_t* out);

    /**
     * @brief 설정 모드 종료(Disable Config) 및 실행 모드 복귀 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 10바이트 이상)
     * @return 기록된 총 바이트 수 (10바이트)
     */
    static size_t buildDisableConfig(uint8_t* out);

    /**
     * @brief 펌웨어 버전 조회(Read Version) 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 10바이트 이상)
     * @return 기록된 총 바이트 수 (10바이트)
     */
    static size_t buildVersionRead(uint8_t* out);

    /**
     * @brief 센서 모듈 재시작(Restart) 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 10바이트 이상)
     * @return 기록된 총 바이트 수 (10바이트)
     */
    static size_t buildRestart(uint8_t* out);

    /**
     * @brief 시스템 동작 파라미터 쓰기 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 16바이트 이상)
     * @param minGate 감지할 최소 게이트 번호 (0 ~ 15)
     * @param maxGate 감지할 최대 게이트 번호 (0 ~ 15)
     * @param timeout 인체 부재 판정 지연 시간 (단위: 초)
     * @param mode 출력 모드 (SIMPLE: 0x0064, ENERGY: 0x0004)
     * @return 기록된 총 바이트 수 (16바이트)
     */
    static size_t buildSysParam(uint8_t* out, uint8_t minGate, uint8_t maxGate, uint16_t timeout, LD2420Mode mode);

    /**
     * @brief 특정 게이트의 움직임 및 정지 감도 임계값 쓰기 명령 프레임 생성
     * @param[out] out 생성된 패킷을 저장할 버퍼 (최소 24바이트 이상)
     * @param gate 대상 게이트 번호 (0 ~ 15)
     * @param moveThresh 움직임 감지 임계값 (0 ~ 100, 낮을수록 민감)
     * @param stillThresh 정지(미세 움직임) 감지 임계값 (0 ~ 100, 낮을수록 민감)
     * @return 기록된 총 바이트 수 (22바이트)
     */
    static size_t buildGateParam(uint8_t* out, uint8_t gate, uint16_t moveThresh, uint16_t stillThresh);

    // =======================================================
    // [프레임 파서 (Parsers)]
    // 수신된 바이트 스트림 버퍼를 분석하여 구조체로 변환합니다.
    // =======================================================

    /**
     * @brief 명령 응답(ACK) 프레임 파싱
     * @param frame 수신된 원시 바이트 버퍼
     * @param len 수신된 데이터 길이 (최소 10바이트 이상 필요)
     * @param[out] cmd 수신된 명령 코드 저장 포인터
     * @param[out] error 수신된 에러/상태 코드 (0이면 성공) 저장 포인터
     * @return 파싱 성공 여부 (true: 유효한 ACK, false: 불완전하거나 비정상 패킷)
     */
    static bool parseAck(const uint8_t* frame, size_t len, uint16_t* cmd, uint16_t* error);

    /**
     * @brief 16개 게이트 에너지 수치가 담긴 바이너리 리포트 프레임 파싱
     * @param frame 수신된 원시 바이트 버퍼
     * @param len 수신된 데이터 길이 (헤더+길이+데이터+푸터 = 45바이트 이상 필요)
     * @param[out] out 파싱 결과를 저장할 LD2420TargetData 구조체 포인터
     * @return 파싱 성공 여부
     */
    static bool parseEnergyFrame(const uint8_t* frame, size_t len, LD2420TargetData* out);

    /**
     * @brief Simple 텍스트 모드("ON Range:xxx", "OFF") 문자열 1라인 파싱
     * @param line 수신된 개행으로 끝난 텍스트 문자열
     * @param len 문자열 길이
     * @param[out] out 파싱 결과를 저장할 LD2420TargetData 구조체 포인터
     * @return 파싱 성공 여부
     */
    static bool parseSimpleLine(const char* line, size_t len, LD2420TargetData* out);

    /**
     * @brief 버전 조회 명령에 대한 응답 프레임에서 ASCII 버전 문자열 추출
     * @param frame 수신된 응답 바이너리 프레임
     * @param len 데이터 길이
     * @param[out] out 버전 정보 저장 구조체 포인터
     * @return 버전 추출 성공 여부
     */
    static bool parseVersion(const uint8_t* frame, size_t len, LD2420VersionInfo* out);

    // =======================================================
    // [유틸리티 (Utilities)]
    // =======================================================

    /**
     * @brief 버퍼 시작이 제어 명령/응답 프레임 헤더(0xFD, 0xFC, 0xFB, 0xFA)인지 검사
     * @param buf 검사할 데이터 버퍼
     * @param len 버퍼 길이 (최소 4바이트 필요)
     * @return true: 명령 프레임 헤더 일치
     */
    static bool isCommandFrame(const uint8_t* buf, size_t len);

    /**
     * @brief 버퍼 시작이 바이너리 에너지 리포트 프레임 헤더(0xF4, 0xF3, 0xF2, 0xF1)인지 검사
     * @param buf 검사할 데이터 버퍼
     * @param len 버퍼 길이 (최소 4바이트 필요)
     * @return true: 에너지 프레임 헤더 일치
     */
    static bool isEnergyFrame(const uint8_t* buf, size_t len);

    /**
     * @brief 수신 버퍼의 길이 필드를 읽어 프레임 전체 길이(헤더+길이+페이로드+푸터) 산출
     * @param buf 최소 6바이트 이상 적재된 버퍼 (4~5번 바이트가 리틀엔디언 길이)
     * @return 프레임 완성에 필요한 총 바이트 수
     */
    static size_t frameTotalLength(const uint8_t* buf);
};

