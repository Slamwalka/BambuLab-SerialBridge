#include "printerbridge.h"

PrinterBridge::PrinterBridge(IPAddress printerIP) : _printerIP(printerIP) {}

void PrinterBridge::ensureConnection(uint8_t index, int port)
{
if (index < 1 || index > 3)
        return;

    if (_clients[index].connected())
        return;

    // Setzt den Client vor dem erneuten Verbindungsaufbau kontrolliert zurück.
    _clients[index].flush();
    delay(2); 
    _clients[index].stop();

    _clients[index].setTimeout(1000);
    _clients[index].connect(_printerIP, port);
}

void PrinterBridge::processPacket(uint8_t channel, uint8_t *data, uint16_t length)
{
    static const int ports[] = {0, 8883, 990, 2024};

    // 101 bis 103 öffnen den jeweiligen Druckerkanal.
    if (channel >= 101 && channel <= 103)
    {
        uint8_t realChan = channel - 100;
        ensureConnection(realChan, ports[realChan]);
        return;
    }

    // 201 bis 203 leeren und schließen den jeweiligen Druckerkanal.
    if (channel >= 201 && channel <= 203)
    {
        uint8_t realChan = channel - 200;

        // Verwirft vor dem Schließen noch ausstehende Druckerdaten.
        while (_clients[realChan].available())
        {
            _clients[realChan].read();
        }

        _clients[realChan].stop();

        return;
    }

    // Kanäle 1 bis 3 enthalten Nutzdaten für den Drucker.
    if (channel < 1 || channel > 3)
        return;

    ensureConnection(channel, ports[channel]);

    // Schreibt die Nutzdaten in höchstens 1024-Byte-Blöcken an den Drucker.
    if (length > 0 && _clients[channel].connected())
    {
        size_t totalWritten = 0;
                 
        while (totalWritten < length && _clients[channel].connected()) 
        {
            size_t toWrite = length - totalWritten;
            if (toWrite > wifiPacketSize) toWrite = wifiPacketSize; 
            
            size_t bytesWritten = _clients[channel].write(data + totalWritten, toWrite);
                         
            if (bytesWritten == 0) {
                delay(1); 
            } else {
                totalWritten += bytesWritten;
            }
        }
    }

    // Bestätigt Kanal 3 erst nach dem Schreiben in den WLAN-Client.
    if (channel == 3)
    {
        Serial.write(0x7E);       
        Serial.write(4);          
        Serial.write((uint8_t)0); 
        Serial.write((uint8_t)1); 
        Serial.write(0x06);       
    }
}

void PrinterBridge::forwardPrinterDataToSerial()
{
    static bool wasConnected[4] = {false, false, false, false};
    
    for (uint8_t i = 1; i <= 3; i++)
    {
        int avail = _clients[i].available();
        bool currentlyConnected = _clients[i].connected();

        // Liest vorhandene Daten auch nach einem Socket-Schluss noch aus.
        if (avail > 0)
        {
            // Begrenzt den einzelnen Lesevorgang auf den konfigurierten Puffer.
            uint8_t buffer[packetSize];
            int bytesToRead = (avail > packetSize) ? packetSize : avail;

            int bytesRead = _clients[i].read(buffer, bytesToRead);

            if (bytesRead > 0)
            {
                Serial.write(0x7E);
                Serial.write(i);
                Serial.write((uint8_t)((bytesRead >> 8) & 0xFF));
                Serial.write((uint8_t)(bytesRead & 0xFF));
                Serial.write(buffer, bytesRead);
            }
        }

        // Meldet EOF erst nach Verbindungsende und vollständiger Pufferleerung.
        if (!currentlyConnected && _clients[i].available() == 0)
        {
            // Meldet das Ende nur für einen zuvor als verbunden beobachteten Kanal.
            if (wasConnected[i])
            {
                Serial.write(0x7E);
                Serial.write(200 + i);
                Serial.write((uint8_t)0);
                Serial.write((uint8_t)0);
                
                wasConnected[i] = false;
            }
        }
        else
        {
            // Hält den logischen Verbindungsstatus bei Restdaten aktiv.
            wasConnected[i] = true;
        }
    }
}

bool PrinterBridge::isAnyConnected()
{
    return _clients[1].connected() || _clients[2].connected() || _clients[3].connected();
}