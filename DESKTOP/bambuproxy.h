#ifndef BAMBUPROXY_H
#define BAMBUPROXY_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QMap>
#include "serialbridge.h"
#include <QPointer>
#include <QTimer>

/**
 * @brief Stellt lokale MQTT- und FTP-Proxykanäle für die Druckerverbindung bereit.
 */
class BambuProxy : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Erzeugt den Proxy und bindet ihn an die serielle Bridge.
     * @param serial Serielle Verbindung zur MCU.
     * @param parent Optionales Qt-Elternelement.
     */
    explicit BambuProxy(SerialBridge *serial, QObject *parent = nullptr);

    /**
     * @brief Gibt die Ressourcen des Proxyobjekts frei.
     */
    ~BambuProxy();

    /**
     * @brief Startet die lokalen TCP-Server und die UDP-Discovery.
     * @return `true`, wenn die benötigten Listener gestartet wurden.
     */
    bool start();

    QString proxyIp = "127.0.0.1";

    // Seriennummer, die in der lokalen Discovery-Antwort verwendet wird.
    QString serialNumber = "";

signals:
    void proxyIpChanged(QString newIp);
    void devSerialChanged(QString newSerial);

private slots:
    // Nimmt neue lokale Verbindungen für die drei Proxy-Kanäle an.
    /**
     * @brief Übernimmt eine neue MQTT-Verbindung.
     */
    void onNewMqttConnection();

    /**
     * @brief Übernimmt eine neue FTP-Kontrollverbindung.
     */
    void onNewFtpControlConnection();

    /**
     * @brief Übernimmt eine neue FTP-Datenverbindung.
     */
    void onNewFtpDataConnection();

    // Leitet Nutzdaten vom lokalen Client zur MCU weiter.
    /**
     * @brief Leitet eingehende MQTT-Daten an die MCU weiter.
     */
    void onMqttClientReadyRead();

    /**
     * @brief Leitet eingehende FTP-Steuerdaten an die MCU weiter.
     */
    void onFtpControlClientReadyRead();

    /**
     * @brief Sammelt und überträgt eingehende FTP-Datenblöcke.
     */
    void onFtpDataClientReadyRead();

    // Reagiert auf das Ende einer lokalen Client-Verbindung.
    /**
     * @brief Trennt den MQTT-Kanal nach Ende der Clientverbindung.
     */
    void onMqttClientDisconnected();

    /**
     * @brief Trennt den FTP-Steuerkanal nach Ende der Clientverbindung.
     */
    void onFtpControlClientDisconnected();

    /**
     * @brief Beendet den FTP-Datenkanal nach Ende der Clientverbindung.
     */
    void onFtpDataClientDisconnected();

    // Verteilt von der MCU empfangene Pakete an den passenden Client.
    /**
     * @brief Leitet ein empfangenes serielles Paket an den passenden Socket weiter.
     * @param channel Kanalnummer des Pakets.
     * @param payload Nutzdaten des Pakets.
     */
    void onSerialPacketReceived(quint8 channel, const QByteArray &payload);

    // Startet beziehungsweise beendet die lokale UDP-Discovery-Antwort.
    /**
     * @brief Startet die Antworten auf UDP-Discovery-Anfragen.
     */
    void startUdpDiscoverySpoofer();

    /**
     * @brief Beendet die lokale UDP-Discovery.
     */
    void stopUdpDiscoverySpoofer();

private:
    SerialBridge *m_serial;

    // Lokale TCP-Listener für MQTT und FTP.
    QTcpServer m_mqttServer;
    QTcpServer m_ftpControlServer;
    QTcpServer m_ftpDataServer;

    // Aktive Kontrollverbindungen auf der Desktop-Seite.
    QTcpSocket *m_mqttClient        = nullptr;
    QTcpSocket *m_ftpControlClient  = nullptr;

    // FTP-Datenverbindungen, indiziert über den Peer-Port.
    QMap<quint16, QTcpSocket*> m_ftpDataClients;

    QByteArray  m_ftpUploadBuffer;
    QTimer      m_uploadTimer;
    bool        m_dataUploadFinished   = false; // Eingangsdaten wurden beendet.
    int         m_eofDelayTicks        = 0;

    bool        m_winscpDisconnected   = false;
    int         m_packetsInFlight      = 0;
    quint64     m_uploadedBytes        = 0;
    /**
     * @brief Sendet den nächsten gepufferten FTP-Datenblock an die MCU.
     */
    void sendNextFtpChunk();
    bool        m_isUpload             = false;

private:
    QTimer      m_ackTimer;  // Überwacht ausstehende Upload-Bestätigungen.
    QTimer      m_dataIdleTimer;
    QByteArray  m_lastChunk;
    int         m_MaxPacketCount       = 2;
    int         m_MaxChunkSize         = 2048;
    bool        m_printerClosedData    = false;
    QUdpSocket  *m_udpSocket;
    QTimer      *m_udpTimer;
};

#endif // BAMBUPROXY_H