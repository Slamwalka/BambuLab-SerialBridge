#include "serialprotocol.h"

SerialProtocol::SerialProtocol() : _state(WAIT_START), _lastChannel(0), _lastLength(0), _payloadIndex(0) {}

bool SerialProtocol::feed(uint8_t b)
{
    static unsigned long lastByteTime = 0;
    if (_state != WAIT_START && (millis() - lastByteTime > 500)) {
        _state = WAIT_START; 
    }
    lastByteTime = millis();

    switch (_state)
    {
    case WAIT_START:
        if (b == START_BYTE)
            _state = READ_CHANNEL;
        break;

    case READ_CHANNEL:
        _lastChannel = b;
        _state = READ_LEN_H;
        break;

    case READ_LEN_H:
        _lastLength = b << 8;
        _state = READ_LEN_L;
        break;

    case READ_LEN_L:
        _lastLength |= b;
        if (_lastLength > 4096)
        { // Verwirft Längen außerhalb des Parserpuffers.
            _state = WAIT_START;
            _lastLength = 0;
            break;
        }

        if(_lastLength == 0)
        {
            _state = WAIT_START;
            return true; // Ein leerer Payload ist ein vollständiges Steuerpaket.
        }

        _payloadIndex = 0;
        _state = READ_PAYLOAD;
        break;

    case READ_PAYLOAD:
        _buffer[_payloadIndex++] = b;
        if (_payloadIndex >= _lastLength)
        {
            _state = WAIT_START;
            return true; // Alle Payload-Bytes wurden empfangen.
        }
        break;
    }
    return false;
}