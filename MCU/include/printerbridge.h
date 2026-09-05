#pragma once
#include <WiFiS3.h>

#define packetSize 2048
#define wifiPacketSize 1024

/**
 * @brief Verbindet die seriellen Bridge-Kanäle mit dem Drucker über WLAN.
 */
class PrinterBridge {
public:
    /**
     * @brief Erzeugt eine Bridge für die angegebene Druckeradresse.
     * @param printerIP IP-Adresse des Druckers.
     */
    PrinterBridge(IPAddress printerIP);

    /**
     * @brief Verarbeitet einen Befehl oder Nutzdaten für einen Bridge-Kanal.
     * @param channel Kanalnummer des seriellen Pakets.
     * @param data Beginn der Nutzdaten.
     * @param length Länge der Nutzdaten in Bytes.
     */
    void processPacket(uint8_t channel, uint8_t* data, uint16_t length);

    /**
     * @brief Überträgt verfügbare Druckerdaten zurück an die MCU.
     */
    void forwardPrinterDataToSerial();

    /**
     * @brief Prüft, ob mindestens ein Druckerkanal verbunden ist.
     * @return `true`, wenn ein Kanal eine aktive Verbindung besitzt.
     */
    bool isAnyConnected();

private:
    IPAddress _printerIP;
    WiFiClient _clients[4]; // Index 0 bleibt ungenutzt; 1 bis 3 sind die Proxy-Kanäle.
    int _activeDataPort = 0;
    
    /**
     * @brief Baut bei Bedarf eine Verbindung für einen Bridge-Kanal auf.
     * @param index Interner Kanalindex zwischen 1 und 3.
     * @param port Zielport des Druckers.
     */
    void ensureConnection(uint8_t index, int port);
    
};