#include "serialbridge.h"
#include <QDebug>
#include <QSerialPortInfo>
#include <QDateTime>

SerialBridge::SerialBridge(QObject *parent)
    : QObject{parent},
      m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead, this, &SerialBridge::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, [=]()
    {
        qDebug() << QDateTime::currentDateTimeUtc().toString() << "SERIALERROR " << m_serial->errorString() << m_serial->error();
        if(m_serial->error() == QSerialPort::SerialPortError::ResourceError)
        {
            m_serial->close();
            reconnTimer.start();
            emit isDeviceDisconnected();
            m_connectionState = false;
        }
    });

    connect(&reconnTimer, &QTimer::timeout, this, &SerialBridge::findDEV);

    reconnTimer.setTimerType(Qt::CoarseTimer);
    reconnTimer.setInterval(2500);
    reconnTimer.start();
}

bool SerialBridge::openConnection(const QString &portName)
{
    if(!portName.isEmpty()) lastCOM = portName;
    m_serial->setPortName(lastCOM);
    m_serial->setBaudRate(128000);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);

    return m_serial->open(QSerialPort::ReadWrite) && m_serial->setDataTerminalReady(true);
}

void SerialBridge::sendPacket(quint8 channel, const QByteArray &payload)
{
    if (payload.size() > 0xFFFF) {
        qWarning() << QDateTime::currentDateTimeUtc().toString() << "Payload zu groß für ein einzelnes Paket!";
        return;
    }

    quint16 len = static_cast<quint16>(payload.size());

    QByteArray frame;
    frame.reserve(HEADER_SIZE + len);

    frame.append(START_BYTE);
    frame.append(channel);

    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));

    frame.append(payload);

    if (m_serial->isOpen()) {
        m_serial->write(frame);
    }
}

void SerialBridge::clearBuffers()
{
    m_buffer.clear();
    if (m_serial && m_serial->isOpen()) {
        m_serial->clear(QSerialPort::AllDirections);
    }
}

void SerialBridge::handleReadyRead()
{
    m_buffer.append(m_serial->readAll());

    // Der Header umfasst Startbyte, Kanal und Länge.
    while (m_buffer.size() >= 4) {
        // Verwirft führende Bytes bis zum Startbyte 0x7E.
        if (static_cast<quint8>(m_buffer.at(0)) != 0x7E) {
            m_buffer.remove(0, 1); // Verwirft führende Störbytes.
            continue;
        }

        // Liest Kanal und Payload-Länge aus dem Header.
        quint8 channel = static_cast<quint8>(m_buffer.at(1));
        quint16 length = (static_cast<quint8>(m_buffer.at(2)) << 8) | static_cast<quint8>(m_buffer.at(3));

        // Kanäle 1 bis 4 und 201 bis 203 sind im Bridge-Protokoll belegt.
        if (channel == 0 || length > 4096) {
            // Ein ungültiger Header wird durch Entfernen des Startbytes übersprungen.
            m_buffer.remove(0, 1);
            continue;
        }

        // Wartet bei einem noch unvollständigen Paket auf weitere serielle Bytes.
        if (m_buffer.size() < 4 + length) {
            break; // Wartet auf den restlichen Frame.
        }

        // Übergibt die vollständige Payload und entfernt den Frame aus dem Puffer.
        QByteArray payload = m_buffer.mid(4, length);
        m_buffer.remove(0, 4 + length);

        // Auch Steuerpakete mit leerer Payload werden an die Anwendung gemeldet.
        emit packetReceived(channel, payload);
    }
}

void SerialBridge::findDEV()
{
    for(const QSerialPortInfo &currentInfo : QSerialPortInfo::availablePorts())
    {
        if(currentInfo.hasProductIdentifier() && currentInfo.hasVendorIdentifier())
        {
            qDebug() << currentInfo.productIdentifier() << currentInfo.vendorIdentifier() << currentInfo.description() << currentInfo.manufacturer() << currentInfo.serialNumber() << currentInfo.portName() << currentInfo.standardBaudRates();

            if(currentInfo.productIdentifier() == 4098 && currentInfo.vendorIdentifier() == 9025)
            {
                if(!m_serial->isOpen())
                {
                     if(!openConnection(currentInfo.portName())) {
                         qCritical() << QDateTime::currentDateTimeUtc().toString() << "BBL WUSB nicht verbunden!";
                         m_connectionState = false;
                     } else {
                         reconnTimer.stop();
                         emit isDeviceConnected();
                         m_connectionState = true;
                     }
                }

                qDebug()<<"VERBUNDEN MIT - " << currentInfo.productIdentifier() << currentInfo.vendorIdentifier() << currentInfo.description() << currentInfo.manufacturer() << currentInfo.serialNumber() << currentInfo.portName() << currentInfo.standardBaudRates();

            }
        }
    }
}


