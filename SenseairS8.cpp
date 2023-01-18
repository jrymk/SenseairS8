#include "SenseairS8.h"

//#define SENSEAIR_S8_DEBUG

SenseairS8::SenseairS8(uint8_t uart) :
        serial(uart) {
}

bool SenseairS8::begin() {
    serial.begin(9600);
    return printSensorInfo();
}

void SenseairS8::initTransaction() {
    serial.flush();
}

void SenseairS8::sendByte(uint8_t data) {
    serial.write(data);
}

bool SenseairS8::readResponse(uint8_t* packet, uint8_t length) {
    uint64_t startTime = esp_timer_get_time();
    uint8_t readBytes = 0;
    delay(50); // sometimes gets response starting with 00 without delay...
    while (serial.available()) {
        if (esp_timer_get_time() > startTime + getResponseTimeout) { // time out
            Serial.printf("Senseair S8: Read exception: Timed out, expected %d bytes, read %d bytes\n",
                          length, readBytes);
            printPacket(packet, length);
            break;
        }

        packet[readBytes++] = serial.read();

        if (readBytes == 3 && packet[1] > 0x80) // exception AND exception code is received
            break;
        if (readBytes == length) // done reading
            break;
    }
#ifdef SENSEAIR_S8_DEBUG
    printPacket(packet, length);
#endif
    if (packet[1] == 0x00 || packet[1] > 0x80) { // exception
        Serial.printf("Senseair S8: Read exception: %2X", packet[1]);
        if (packet[2] == 0x01)
            Serial.printf(" (Illegal function)");
        if (packet[2] == 0x02)
            Serial.printf(" (Illegal data address)");
        if (packet[2] == 0x03)
            Serial.printf(" (Illegal data value)");
        Serial.printf("\n");
        printPacket(packet, length);
        return false;
    }
    /// TODO: check CRC? (nah)
    if (readBytes == length)
        return true;
    return false; // timeout
}

void SenseairS8::printPacket(uint8_t* packet, uint8_t length) {
    Serial.printf("Senseair S8: Response:");
    for (int i = 0; i < length; i++)
        Serial.printf(" %2X", packet[i]);
    Serial.printf("\n");
}

SenseairS8::SensorStatus SenseairS8::readSensorStatus() {
    SensorStatus result;
    initTransaction();
    sendByte(0xFE); // any sensor
    sendByte(0x04); // read input registers
    sendByte(0x00); // start address 0x0000
    sendByte(0x00);
    sendByte(0x00); // quantity of registers 4 (meter status, alarm status, output status, space co2);
    sendByte(0x04);
    sendByte(0xE5); // crc 0xC6E5
    sendByte(0xC6);
    // address field 0xFE (1) + function code 0x04 (1) + byte count (1) + register value (8 = 4 registers) + crc(2)
    uint8_t packet[13];
    if (readResponse(packet, 13)) {
        if (packet[0] == 0xFE && packet[1] == 0x04 && packet[2] == 0x08) {
            result.isValid = true;
            result.fatalError = packet[4] & (1 << 0);
            result.offsetRegulationError = packet[4] & (1 << 1);
            result.algorithmError = packet[4] & (1 << 2);
            result.outputError = packet[4] & (1 << 3);
            result.selfDiagnosticsError = packet[4] & (1 << 4);
            result.outOfRange = packet[4] & (1 << 5);
            result.memoryError = packet[4] & (1 << 6);
            result.anyError = result.fatalError || result.offsetRegulationError || result.algorithmError || result.outputError || result.selfDiagnosticsError || result.memoryError;
            result.alarmOutputStatus = packet[8] & (1 << 0);
            result.pwmOutputStatus = packet[8] & (1 << 1);
            result.co2 = (uint16_t(packet[9]) << 8) | packet[10];
            result.crc = (uint16_t(packet[11]) << 8) | packet[12];
        }
        else {
            Serial.printf("Senseair S8: Read sensor status failed\n");
            printPacket(packet, 13);
        }
    }
    return result;
}

SenseairS8::SensorInfo SenseairS8::readSensorInfo() {
    SensorInfo result;
    initTransaction();
    sendByte(0xFE); // any sensor
    sendByte(0x04); // read input registers
    sendByte(0x00); // start address 0x0025
    sendByte(0x19);
    sendByte(0x00); // quantity of registers 6;
    sendByte(0x06);
    sendByte(0xB5); // crc 0xC0B5
    sendByte(0xC0);
    // address field 0xFE (1) + function code 0x04 (1) + byte count (1) + register value (12 = 6 registers) + crc(2)
    uint8_t packet[17];
    if (readResponse(packet, 17)) {
        if (packet[0] == 0xFE && packet[1] == 0x04 && packet[2] == 0x0C) {
            result.isValid = true;
            result.sensorTypeID = (uint32_t(packet[3]) << 24) | (uint32_t(packet[4]) << 16) | (uint32_t(packet[5]) << 8) | packet[6];
            result.memoryMapVersion = (uint16_t(packet[7]) << 8) | packet[8];
            result.fwVersion = (uint16_t(packet[9]) << 8) | packet[10];
            result.sensorID = (uint32_t(packet[11]) << 24) | (uint32_t(packet[12]) << 16) | (uint32_t(packet[13]) << 8) | packet[14];
            result.crc = (packet[15] << 8) | packet[16];
        }
        else {
            Serial.printf("Senseair S8: Read sensor status failed\n");
            printPacket(packet, 17);
        }
    }
    return result;
}

uint16_t SenseairS8::readABCPeriod() {
    initTransaction();
    sendByte(0xFE); // any sensor
    sendByte(0x03); // read holding registers
    sendByte(0x00); // start address 0x001F
    sendByte(0x1F);
    sendByte(0x00); // quantity of registers 1
    sendByte(0x01);
    sendByte(0xA1); // crc 0xC3A1
    sendByte(0xC3);
    // address field 0xFE (1) + function code 0x03 (1) + byte count (1) + register value (2 = 1 register) + crc(2)
    uint8_t packet[7];
    if (readResponse(packet, 7)) {
        if (packet[0] == 0xFE && packet[1] == 0x03 && packet[2] == 0x02)
            return (uint16_t(packet[3]) << 8) | packet[4];
        else {
            Serial.printf("Senseair S8: Read ABC period failed\n");
            printPacket(packet, 7);
        }
    }
    return 0xFFFF;
}

bool SenseairS8::setABCPeriod(uint16_t periodInHours, uint16_t crc) {
    if (periodInHours != 180 && crc == 0x74AC && periodInHours != 0) {
        Serial.printf("Senseair S8: Set ABC period failed: Please supply CRC value");
        return false;
    }
    if (periodInHours == 0)
        crc = 0x03AC;
    sendByte(0xFE); // any sensor
    sendByte(0x06); // write single register
    sendByte(0x00); // address 0x001F
    sendByte(0x1F);
    sendByte((periodInHours >> 8) & 0xFF); // value
    sendByte(periodInHours & 0xFF);
    sendByte(crc & 0xFF); // crc (sent low byte first)
    sendByte((crc >> 8) & 0xFF);
    // address field 0xFE (1) + function code 0x06 (1) + address (2) + value (2) + crc(2) (an echo)
    uint8_t packet[8];
    if (readResponse(packet, 8)) {
        if (packet[0] == 0xFE && packet[1] == 0x06 && packet[2] == 0x00 && packet[3] == 0x1F
            && ((uint16_t(packet[4]) << 8) | packet[5]) == periodInHours
            && ((uint16_t(packet[7]) << 8) | packet[6]) == crc)
            return true;
        else {
            Serial.printf("Senseair S8: Set ABC period failed\n");
            printPacket(packet, 8);
        }
    }
    return false;
}

bool SenseairS8::printSensorInfo() {
    SensorStatus status = readSensorStatus();
    SensorInfo info = readSensorInfo();
    uint16_t abcPeriod = readABCPeriod();
    if (!status.isValid || !info.isValid || abcPeriod == 0xFFFF)
        Serial.printf("Senseair S8: No sensor info: Failed to read data\n");
    Serial.printf("Senseair S8: FULL SENSOR INFO\n");
    Serial.printf("      Sensor Type ID: %8X\n", info.sensorTypeID);
    Serial.printf("           Sensor ID: %8X\n", info.sensorID);
    Serial.printf("    Firmware Version: %d.%d\n", (info.fwVersion >> 4) & 0x0F, info.fwVersion & 0x0F);
    Serial.printf("  Memory Map Version: %d\n", info.memoryMapVersion);
    Serial.printf("           Space CO2: %dppm\n", status.co2);
    Serial.printf("        Meter Status: \n");
    if (status.fatalError) Serial.printf("FATAL_ERROR ");
    if (status.offsetRegulationError) Serial.printf("OFFSET_REGULATION_ERROR ");
    if (status.algorithmError) Serial.printf("ALGORITHM_ERROR ");
    if (status.outputError) Serial.printf("OUTPUT_ERROR ");
    if (status.selfDiagnosticsError) Serial.printf("SELF_DIAGNOSTICS_ERROR ");
    if (status.outOfRange) Serial.printf("OUT_OF_RANGE ");
    if (status.memoryError) Serial.printf("MEMORY_ERROR ");
    Serial.printf(" Alarm Output Status: %s\n", status.alarmOutputStatus ? "true" : "false");
    Serial.printf("   PWM Output Status: %s\n", status.pwmOutputStatus ? "true" : "false");
    Serial.printf("          ABC Period: %d hours\n", abcPeriod);
    return status.isValid && info.isValid && abcPeriod != 0xFFFF;
}

