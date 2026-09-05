#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QStyle>

#include "bambuproxy.h"
#include "serialbridge.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    SerialBridge *bridge = new SerialBridge;
    BambuProxy proxy(bridge);
    proxy.start();

    // Das Programm arbeitet ohne Hauptfenster über das System-Tray-Menü.

    QSystemTrayIcon trayIcon(QIcon(":/ICO/Logo.png"));
    trayIcon.setToolTip("BBL Serialbridge");

    QMenu trayMenu;

    // Diese Aktionen zeigen Statusinformationen an und sind nicht editierbar.
    QAction *statusAction = new QAction("Status: Suche Brücke...", &trayMenu);
    statusAction->setEnabled(false);
    trayMenu.addAction(statusAction);

    QAction *statusPrinterAction = new QAction("Drucker SN: ", &trayMenu);
    statusPrinterAction->setEnabled(false);
    trayMenu.addAction(statusPrinterAction);

    QAction *statusProxyAction = new QAction("Proxy IP: ", &trayMenu);
    statusProxyAction->setEnabled(false);
    trayMenu.addAction(statusProxyAction);


    trayMenu.addSeparator();

    // Beendet die Qt-Anwendung über den einzigen aktiven Menüeintrag.
    QAction *quitAction = new QAction("Beenden", &trayMenu);
    QObject::connect(quitAction, &QAction::triggered, &a, &QApplication::quit);
    trayMenu.addAction(quitAction);

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Aktualisiert Tray-Status und Benachrichtigungen aus den Bridge-Signalen.

    QObject::connect(bridge, &SerialBridge::isDeviceConnected, [statusAction, &trayIcon]() {
        statusAction->setText("Status: Drucker Verbunden");
        // Die Verbindung wird zusätzlich als Tray-Benachrichtigung gemeldet.
        trayIcon.showMessage("BBL Serialbridge", "Brücke erfolgreich verbunden!", QSystemTrayIcon::Information, 2000);
    });

    QObject::connect(bridge, &SerialBridge::isDeviceDisconnected, [statusAction, &trayIcon]() {
        statusAction->setText("Status: Suche Brücke...");
        trayIcon.showMessage("BBL Serialbridge", "Verbindung zur Brücke verloren.", QSystemTrayIcon::Warning, 2000);
    });

    QObject::connect(&proxy, &BambuProxy::proxyIpChanged, [&trayIcon, statusProxyAction](QString newText) {
        statusProxyAction->setText("Proxy IP: " + newText);
        trayIcon.showMessage("BBL Serialbridge", "Spoofe UDP auf IP: " + newText, QSystemTrayIcon::Information, 2000);
    });

    QObject::connect(&proxy, &BambuProxy::devSerialChanged, [&trayIcon, statusPrinterAction](QString newText) {
        statusPrinterAction->setText("Drucker SN: " + newText);
        trayIcon.showMessage("BBL Serialbridge", "Verbindung zu DruckerSN: " + newText, QSystemTrayIcon::Information, 2000);
    });

    statusPrinterAction->setText("Drucker SN: " + proxy.serialNumber);

    return QApplication::exec();
}