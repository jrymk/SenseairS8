#ifndef SENSEAIRS8_H
#define SENSEAIRS8_H

#include <Arduino.h>
#include <HardwareSerial.h>

class SenseairS8 {
public:
    SenseairS8(uint8_t uart);

    // IR1-4
    /// @brief CO2 concentration and statuses
    struct SensorStatus {
        bool isValid = false;
        bool fatalError = false;
        bool offsetRegulationError = false;
        bool algorithmError = false;
        bool outputError = false;
        bool selfDiagnosticsError = false;
        bool outOfRange = false;
        bool memoryError = false;
        bool anyError = false;
        bool alarmOutputStatus = false;
        bool pwmOutputStatus = false;
        // space co2 concentration in ppm
        uint16_t co2 = 0;
        uint16_t crc = 0xFFFF;
    };

    // IR26 - 31
    /// @brief Sensor ID and FW version
    struct SensorInfo {
        bool isValid = false;
        uint32_t sensorTypeID = 0xFFFFFFFF;
        uint16_t memoryMapVersion = 0xFFFF;
        // high byte is FW Main revision, low byte is FW Sub revision
        uint16_t fwVersion = 0xFFFF;
        // 4 bytes sensor's serial number
        uint32_t sensorID = 0xFFFFFFFF;
        uint16_t crc = 0xFFFF;
    };

    bool begin();

    SensorStatus readSensorStatus();

    SensorInfo readSensorInfo();

    // returns ABC period in hours, returns 0xFFFF if failed, returns 0 if ABC is disabled
    uint16_t readABCPeriod();

    // sets ABC period in hours, by default it is 7.5 days or 180 hours, leave parameters empty to set back to default.
    // to set to other values, calculate CRC-16 MODBUS of 0xFE06001F{period 2 bytes}
    // to disable ABC, set to 0, the crc for 0xFE06001F0000 is 0x03AC. you can leave this empty if set to 0
    bool setABCPeriod(uint16_t periodInHours = 180, uint16_t crc = 0x74AC);

    bool printSensorInfo();

private:
    HardwareSerial serial;

    uint32_t getResponseTimeout = 300000; // 300ms

    void initTransaction();

    void sendByte(uint8_t data);

    bool readResponse(uint8_t* packet, uint8_t length);

    void printPacket(uint8_t* packet, uint8_t length);
};

#endif //SENSEAIRS8_H
