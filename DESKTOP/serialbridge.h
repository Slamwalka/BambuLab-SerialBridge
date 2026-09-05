#pragma once

#include<QObject>
#include<QSerialPort>
#include<QByteArray>
#include<QTimer>

/**
 * @brief Kapselt die serielle Verbindung zwischen Desktop und MCU.
 */
class SerialBridge : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Erzeugt eine serielle Bridge mit leerem Verbindungsstatus.
     * @param parent Optionales Qt-Elternelement.
     */
    explicit SerialBridge(QObject *parent = nullptr);

    /**
     * @brief Öffnet den angegebenen seriellen Port.
     * @param portName Name des zu öffnenden Ports.
     * @return `true`, wenn der Port geöffnet werden konnte.
     */
    bool openConnection(const QString &portName);

    /**
     * @brief Verpackt und sendet Nutzdaten über den seriellen Kanal.
     * @param channel Zielkanal des Bridge-Protokolls.
     * @param payload Zu übertragende Nutzdaten.
     */
    void sendPacket(quint8 channel, const QByteArray &payload);

    /**
     * @brief Leert Eingangs- und Ausgangspuffer der seriellen Verbindung.
     */
    void clearBuffers();

    /**
     * @brief Liefert den zuletzt bekannten Verbindungsstatus.
     * @return `true`, wenn das serielle Gerät als verbunden gilt.
     */
    bool deviceConnectionState() { return m_connectionState; }

signals:
    void packetReceived(quint8 channel, const QByteArray &payload);
    void isDeviceConnected();
    void isDeviceDisconnected();

private slots:
    /**
     * @brief Liest serielle Bytes ein und verarbeitet vollständige Frames.
     */
    void handleReadyRead();

    /**
     * @brief Sucht nach einem verfügbaren seriellen Gerät.
     */
    void findDEV();

private:
    QSerialPort *m_serial;
    QByteArray m_buffer;

    static constexpr char START_BYTE    = 0x7E;
    static constexpr int  HEADER_SIZE   = 4;

    QTimer reconnTimer;

    QString lastCOM;
    bool m_connectionState = false;
};

