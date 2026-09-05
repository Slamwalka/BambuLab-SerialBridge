#include "BambuProxy.h"
#include <QDebug>
#include <QRegularExpression>
#include <QThread>
#include <QDateTime>
#include <QNetworkInterface>

bool BambuProxy::isAllowedListenAddress(const QString &address, bool allowLanAccess)
{
    if (address.isEmpty()) {
        return false;
    }

    const QHostAddress candidate(address);
    if (candidate.isNull() || candidate.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }

    if (candidate == QHostAddress::LocalHost || candidate == QHostAddress("127.0.0.1")) {
        return true;
    }

    if (!allowLanAccess) {
        return false;
    }

    const QString ip = candidate.toString();
    if (ip.startsWith("192.168.")) {
        return true;
    }
    if (ip.startsWith("10.")) {
        return true;
    }
    if (ip.startsWith("172.")) {
        const QString remainder = ip.mid(4);
        const int dotIndex = remainder.indexOf('.');
        if (dotIndex > 0) {
            const int secondOctet = remainder.left(dotIndex).toInt();
            return secondOctet >= 16 && secondOctet <= 31;
        }
    }

    return false;
}

bool BambuProxy::setListenAddress(const QString &bindAddress, bool allowLanAccess)
{
    if (!isAllowedListenAddress(bindAddress, allowLanAccess)) {
        m_listenAddress = "127.0.0.1";
        m_allowLanAccess = false;
        return false;
    }

    m_listenAddress = bindAddress;
    m_allowLanAccess = allowLanAccess;
    return true;
}

BambuProxy::BambuProxy(SerialBridge *serial, QObject *parent)
    : QObject(parent), m_serial(serial)
{
    connect(m_serial, &SerialBridge::packetReceived, this, &BambuProxy::onSerialPacketReceived);
    connect(m_serial, &SerialBridge::isDeviceConnected, this, &BambuProxy::startUdpDiscoverySpoofer);
    connect(m_serial, &SerialBridge::isDeviceDisconnected, this, &BambuProxy::stopUdpDiscoverySpoofer);

    m_dataIdleTimer.setSingleShot(true);
        connect(&m_dataIdleTimer, &QTimer::timeout, this, [this]() {
            qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch())
                    << ">>> Keine Daten mehr Empfangen, schließe Verbindung";

            if (!m_printerClosedData) {
                qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Data] Sende EOF an Drucker.";
                m_serial->sendPacket(203, QByteArray());
                m_printerClosedData = true;

                for (QTcpSocket *client : std::as_const(m_ftpDataClients)) {
                    if (client) client->disconnectFromHost();
                }
            }
        });
}

BambuProxy::~BambuProxy()
{
    for (QTcpSocket *client : std::as_const(m_ftpDataClients)) {
        if (client) client->disconnectFromHost();
    }

    m_serial->sendPacket(201, QByteArray());
    m_serial->sendPacket(202, QByteArray());
    m_serial->sendPacket(203, QByteArray());
}

bool BambuProxy::start()
{
    const QHostAddress bindAddress(m_listenAddress);

    if (bindAddress.isNull() || !isAllowedListenAddress(m_listenAddress, m_allowLanAccess)) {
        qWarning() << QDateTime::currentDateTimeUtc().toString() << "Ungültige Bindungsadresse, verwende Standard 127.0.0.1.";
        m_listenAddress = "127.0.0.1";
        m_allowLanAccess = false;
    }

    // MQTT wird auf der explizit gewählten lokalen Bindungsadresse angenommen.
    if (!m_mqttServer.listen(QHostAddress(m_listenAddress), 8883)) {
        qCritical() << QDateTime::currentDateTimeUtc().toString() << "Fehler: Konnte MQTT Server auf" << m_listenAddress << ":8883 nicht starten!";
        return false;
    }
    connect(&m_mqttServer, &QTcpServer::newConnection, this, &BambuProxy::onNewMqttConnection);
    qInfo() << QDateTime::currentDateTimeUtc().toString() << "MQTT Server lauscht auf" << m_listenAddress << ":8883";

    // FTP-Steuerdaten werden auf der selben lokalen Bindungsadresse angenommen.
    if (!m_ftpControlServer.listen(QHostAddress(m_listenAddress), 990)) {
        qCritical() << QDateTime::currentDateTimeUtc().toString() << "Fehler: Konnte FTP Control Server auf" << m_listenAddress << ":990 nicht starten!";
        return false;
    }
    connect(&m_ftpControlServer, &QTcpServer::newConnection, this, &BambuProxy::onNewFtpControlConnection);
    qInfo() << QDateTime::currentDateTimeUtc().toString() << "FTP Control Server lauscht auf" << m_listenAddress << ":990";

    // FTP-Daten werden über den gleichen lokalen Bindungshost akzeptiert.
    if (!m_ftpDataServer.listen(QHostAddress(m_listenAddress), 2024)) {
        qWarning() << QDateTime::currentDateTimeUtc().toString() << "Warnung: FTP Data Server auf" << m_listenAddress << ":2024 konnte nicht gestartet werden.";
    } else {
        connect(&m_ftpDataServer, &QTcpServer::newConnection, this, &BambuProxy::onNewFtpDataConnection);
        qInfo() << QDateTime::currentDateTimeUtc().toString() << "FTP Data Server lauscht auf" << m_listenAddress << ":2024";
    }

    return true;
}

// Kanal 1 transportiert MQTT-Daten zwischen Client und MCU.
void BambuProxy::onNewMqttConnection()
{
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[MQTT] Neue Verbindung.";
    if (m_mqttClient) {
        m_mqttClient->disconnect();
        m_mqttClient->close();
        m_mqttClient->deleteLater();
    }

    m_mqttClient = m_mqttServer.nextPendingConnection();

    connect(m_mqttClient, &QTcpSocket::readyRead, this, &BambuProxy::onMqttClientReadyRead);
    connect(m_mqttClient, &QTcpSocket::disconnected, this, &BambuProxy::onMqttClientDisconnected);

    // Der MCU-Verbindungsbefehl für Kanal 1 wird vor den Nutzdaten gesendet.
    m_serial->sendPacket(101, QByteArray());
}

void BambuProxy::onMqttClientReadyRead()
{
    if (!m_mqttClient) return;
    m_serial->sendPacket(1, m_mqttClient->readAll());
}

void BambuProxy::onMqttClientDisconnected()
{
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[MQTT] Client getrennt.";
    m_serial->sendPacket(201, QByteArray()); // Trennt Kanal 1 auf der MCU.

    if (m_mqttClient) {
        m_mqttClient->deleteLater();
        m_mqttClient = nullptr;
    }
}

// Kanal 2 transportiert FTP-Steuerdaten zwischen Client und MCU.
void BambuProxy::onNewFtpControlConnection()
{
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Ctrl] Neue Kontrollverbindung.";

    // Eine vorherige Kontrollverbindung wird vor der Annahme ersetzt.
    if (m_ftpControlClient)
    {
        m_ftpControlClient->disconnect();
        m_ftpControlClient->close();
        m_ftpControlClient->deleteLater();
    }

    m_ftpControlClient = m_ftpControlServer.nextPendingConnection();

    connect(m_ftpControlClient, &QTcpSocket::readyRead, this, [this]() {
        if (m_ftpControlClient) {
            QByteArray data = m_ftpControlClient->readAll();
            m_serial->sendPacket(2, data); // Leitet FTP-Steuerdaten an Kanal 2 weiter.
        }
    });

    // Der Trennbefehl folgt erst beim Abbau durch den FTP-Client.
    connect(m_ftpControlClient, &QTcpSocket::disconnected, this, [this]() {
        qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Ctrl] Client getrennt. Schließe Arduino-Socket (202)...";
        m_serial->sendPacket(202, QByteArray()); // Trennt den FTP-Steuerkanal.

        if (m_ftpControlClient) {
            m_ftpControlClient->deleteLater();
            m_ftpControlClient = nullptr;
        }
    });

    // Baut den MCU-Tunnel für den Kontrollkanal auf.
    m_serial->sendPacket(102, QByteArray());
}

void BambuProxy::onFtpControlClientReadyRead()
{
    if (!m_ftpControlClient) return;
    QByteArray data = m_ftpControlClient->readAll();
    m_serial->sendPacket(2, data); // Leitet FTP-Steuerdaten an Kanal 2 weiter.
}

void BambuProxy::onFtpControlClientDisconnected()
{
    qInfo() << QDateTime::currentDateTimeUtc().toString() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Ctrl] Client getrennt.";
    m_serial->sendPacket(202, QByteArray()); // Trennt den FTP-Steuerkanal.
    if (m_ftpControlClient) {
        m_ftpControlClient->deleteLater();
        m_ftpControlClient = nullptr;
    }
} // Ende der FTP-Kontrolltrennung.

// Kanal 3 transportiert FTP-Daten und Upload-Bestätigungen.
void BambuProxy::onNewFtpDataConnection()
{
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Data] Neue Datenverbindung.";

    // Vor dem nächsten Transfer werden alte Sockets und Puffer entfernt.
    for (QTcpSocket *oldClient : std::as_const(m_ftpDataClients)) {
        if (oldClient) {
            oldClient->disconnect();
            oldClient->close();
            oldClient->deleteLater();
        }
    }
    m_ftpDataClients.clear();
    m_ftpUploadBuffer.clear();

    // Transferstatus für eine neue FTP-Datenverbindung zurücksetzen.
    m_dataUploadFinished = false;
    m_packetsInFlight = 0;
    m_isUpload = false;
    m_printerClosedData = false;
    m_uploadedBytes = 0;
    m_winscpDisconnected = false;

    // Den nächsten wartenden Datenclient übernehmen.
    QTcpSocket *client = m_ftpDataServer.nextPendingConnection();
    quint16 port = client->peerPort();
    m_ftpDataClients[port] = client;

    client->setReadBufferSize(2048);
    client->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 2048);

    // Lese-, Abschluss- und Trennsignale des Datenclients verknüpfen.
    connect(client, &QTcpSocket::readyRead, this, [this, client]() {
        if (client) sendNextFtpChunk();
    });

    auto finishTransfer = [this, client]() {
        if (client && client->bytesAvailable() > 0) {
            QByteArray rest = client->readAll();
            m_uploadedBytes += rest.size();
            m_ftpUploadBuffer.append(rest);
        }
        m_dataUploadFinished = true;

        if (m_packetsInFlight < m_MaxPacketCount && !m_printerClosedData) {
            sendNextFtpChunk();
        }
    };

    connect(client, &QTcpSocket::readChannelFinished, this, finishTransfer);
    connect(client, &QTcpSocket::disconnected, this, [this, client, finishTransfer]()
    {
        finishTransfer();
        m_winscpDisconnected = true;

        if (client) {
            m_ftpDataClients.remove(client->peerPort());
            client->deleteLater();
        }
    });

    // MCU-Verbindung anfordern und den Leerlauftimer starten.
    m_serial->sendPacket(103, QByteArray());

    m_dataIdleTimer.start(10000);
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch())
            << ">>> Neuer Datenkanal. Reset! Timer auf 10000 gestartet.";
}
void BambuProxy::onFtpDataClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // Eingehende Uploaddaten zunächst im Anwendungspuffer sammeln.
    m_ftpUploadBuffer.append(client->readAll());
}

void BambuProxy::onFtpDataClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Data] WinSCP hat das Senden beendet. Warte auf Puffer-Leerung...";

    // Restdaten aus dem Socket vor dem endgültigen Abbau übernehmen.
    if (client->bytesAvailable() > 0) {
        m_ftpUploadBuffer.append(client->readAll());
    }

    // Markiert das Ende der Eingabe für die spätere EOF-Übertragung.
    m_dataUploadFinished = true;

    m_ftpDataClients.remove(client->peerPort());
    client->deleteLater();
}

// Verteilt MCU-Pakete anhand ihres Kanals an lokale TCP-Sockets.
void BambuProxy::onSerialPacketReceived(quint8 channel, const QByteArray &payload)
{
    if(payload.isEmpty() && channel < 201) return;

    switch(channel)
    {
    case 1: // MQTT-Kanal.
        if(m_mqttClient && m_mqttClient->isOpen()) m_mqttClient->write(payload);
        return;
    case 2: // FTP-Steuerkanal.
        if(m_ftpControlClient && m_ftpControlClient->isOpen()) m_ftpControlClient->write(payload);
        return;
    case 3: // FTP-Datenkanal.
        for(QTcpSocket *client : std::as_const(m_ftpDataClients)) {
                    if(client) client->write(payload);
                }

                if(!m_isUpload) {
                    // Für Downloads wird der Leerlauftimer bei Datenempfang verlängert.
                    m_dataIdleTimer.start(5000);
                } else {
                    qInfo() << "Case 3: WARNUNG! m_isUpload ist true. Timer wird NICHT gestartet!";
                }
                return;
    case 4: // Bestätigung für Upload-Daten.
        if(payload.at(0) == 0x06)
        {
            m_ackTimer.stop();
            if(m_packetsInFlight > 0) m_packetsInFlight--;
            sendNextFtpChunk();
        }
        return;
    case 201:
        qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[MQTT] EOF vom Drucker empfangen. Schließe Verbindung.";
        if (m_mqttClient) m_mqttClient->disconnectFromHost();
        return;
    case 202:
        qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Ctrl] EOF vom Drucker empfangen. Schließe Verbindung.";
        if (m_ftpControlClient) m_ftpControlClient->disconnectFromHost();

        return;
    case 203:
        qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Data] EOF vom Drucker empfangen. Schließe Datenverbindung.";
        m_printerClosedData = true; // Sperrt weitere Datenübertragungen.
        for (QTcpSocket *client : std::as_const(m_ftpDataClients))
                {
                    if (client) {
                        if (!m_isUpload) {
                            qInfo() << "Download beendet. Schließe Datenverbindung aktiv.";
                            client->disconnectFromHost();
                        } else {
                            qInfo() << "Upload beendet. Überlasse Bambu Studio den TLS-Shutdown.";
                        }
                    }
                }
        m_dataIdleTimer.stop();
        return;
    };
}

void BambuProxy::startUdpDiscoverySpoofer()
{
    m_udpSocket = new QUdpSocket(this);
    m_udpTimer = new QTimer(this);

    connect(m_udpTimer, &QTimer::timeout, this, [this]()
            {
        // Ermittelt eine verwendbare lokale IPv4-Adresse für die Discovery-Antwort.
        for (const QHostAddress &ip : QNetworkInterface::allAddresses()) {
                    if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip != QHostAddress::LocalHost) {
                        QString ipString = ip.toString();
                        if (!ipString.startsWith("169.254.") && ipString.startsWith("192.168.")) {
                            if(proxyIp != ipString)
                            {
                                proxyIp = ipString;
                                emit proxyIpChanged(proxyIp);
                            }

                            break;
                        }
                    }
                }

        QByteArray payload;
        payload.append("HTTP/1.1 200 OK\r\n");
        payload.append("Server: Buildroot/2018.02-rc3 UPnP/1.0 ssdpd/1.8\r\n");
        payload.append("Date: Wed, 02 Sep 2026 12:00:00 GMT\r\n");
        payload.append("Location: " + proxyIp.toUtf8() + "\r\n");
        payload.append("ST: urn:bambulab-com:device:3dprinter:1\r\n");
        payload.append("EXT:\r\n");
        payload.append("USN: " + serialNumber.toUtf8() + "\r\n");
        payload.append("Cache-Control: max-age=1800\r\n");

        // Das Discovery-Feld meldet das im Payload festgelegte Gerätemodell.
        payload.append("DevModel.bambu.com: C12\r\n");

        payload.append("DevName.bambu.com: KTC-ENTW_P1S\r\n");
        payload.append("DevSignal.bambu.com: -44\r\n");
        payload.append("DevConnect.bambu.com: lan\r\n");
        payload.append("DevBind.bambu.com: free\r\n\r\n");

        // Sendet die Discovery-Antwort lokal und als Broadcast an Port 2021.
        m_udpSocket->writeDatagram(payload, QHostAddress::LocalHost, 2021);

        m_udpSocket->writeDatagram(payload, QHostAddress::Broadcast, 2021);
    });

    // Der Timer wiederholt die Discovery-Antwort alle drei Sekunden.
    m_udpTimer->start(3000);
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[UDP-Spoofer] Sende Discovery-Broadcasts auf Port 2021...";
}

void BambuProxy::stopUdpDiscoverySpoofer()
{
    qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[UDP-Spoofer] Beende Discovery-Broadcasts auf Port 2021...";
    m_udpTimer->stop();
    m_udpTimer->disconnect();
    m_udpTimer->deleteLater();
}

void BambuProxy::sendNextFtpChunk()
{
    if (m_printerClosedData) return;

    while (m_packetsInFlight < m_MaxPacketCount)
    {
        QByteArray chunk;
        int chunkSize = m_MaxChunkSize / m_MaxPacketCount;

        switch(m_ftpUploadBuffer.size())
        {
        case 0:
            for (QTcpSocket *client : std::as_const(m_ftpDataClients))
            {
                if (client && client->bytesAvailable() > 0) {
                    chunk = client->read(chunkSize);
                    m_uploadedBytes += chunk.size();
                    break;
                }
            }
            break;
        default:
            chunk = m_ftpUploadBuffer.left(chunkSize);
            m_ftpUploadBuffer.remove(0, chunk.size());
            break;
        }

        if(!chunk.isEmpty())
        {
            if(m_uploadedBytes > 4096)
            {
                qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "ACHTUNG: 4KB überschritten! Timer wird gestoppt. Upload erkannt";
                m_isUpload = true;
                m_dataIdleTimer.stop();
            }
        }

        if (chunk.isEmpty()) {
            if (m_dataUploadFinished && m_packetsInFlight == 0 && !m_printerClosedData) {

                qInfo() << QString::number(QDateTime::currentDateTimeUtc().currentMSecsSinceEpoch()) << "[FTP-Data]  Schließe Kanal 3 (Sende EOF)...";
                m_serial->sendPacket(203, QByteArray());
                m_printerClosedData = true;
                m_winscpDisconnected = false;
                m_dataUploadFinished = false;
            }
            return;
        }

        // Sendet den Chunk und erhöht die Zahl ausstehender Bestätigungen.
        m_packetsInFlight++;
        m_lastChunk = chunk;
        m_serial->sendPacket(3, chunk);
    }
}