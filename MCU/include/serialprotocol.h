#pragma once

#include <Arduino.h>

/**
 * @brief Parst serielle Frames der Bridge byteweise.
 */
class SerialProtocol {
public:
    /**
     * @brief Erzeugt einen Parser im Zustand für das nächste Startbyte.
     */
    SerialProtocol();

    /**
     * @brief Übergibt ein Byte an den Zustandsautomaten.
     * @param b Empfangener Bytewert.
     * @return `true`, sobald ein vollständiges Paket vorliegt.
     */
    bool feed(uint8_t b);

    /**
     * @brief Liefert den Kanal des zuletzt vollständig empfangenen Pakets.
     */
    uint8_t getChannel() const { return _lastChannel; }

    /**
     * @brief Liefert die Payload-Länge des letzten Pakets.
     */
    uint16_t getLength() const { return _lastLength; }

    /**
     * @brief Liefert den Puffer mit der Payload des letzten Pakets.
     */
    uint8_t* getPayload() { return _buffer; }

private:
    enum State { WAIT_START, READ_CHANNEL, READ_LEN_H, READ_LEN_L, READ_PAYLOAD };
    
    State _state;
    uint8_t _lastChannel;
    uint16_t _lastLength;
    uint16_t _payloadIndex;
    uint8_t _buffer[4096];
    
    static constexpr uint8_t START_BYTE = 0x7E;
};