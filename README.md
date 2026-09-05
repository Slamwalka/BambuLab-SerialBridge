# BambuLab-PrinterBridge

## Funktion

Der BambuLab-PrinterBridge stellt eine lokale Verbindung zwischen einem
Desktop-Rechner und einem Bambu-Lab-Drucker her. Die Desktop-Anwendung nimmt
die für den Drucker benötigten MQTT- und FTP-Verbindungen lokal an und
überträgt deren Daten über eine serielle Verbindung an die MCU.

Die MCU läuft auf einem Arduino UNO R4 WiFi. Sie stellt ein eigenes WLAN als
Access Point bereit, verbindet sich darüber mit dem Drucker und leitet die
Daten der drei Bridge-Kanäle weiter:

- MQTT für die Druckerkommunikation,
- FTP-Steuerdaten für die FTP-Verbindung,
- FTP-Daten für Uploads und Downloads.

Druckerdaten werden von der MCU über dieselbe serielle Verbindung zurück an
die Desktop-Anwendung gesendet. Zusätzlich veröffentlicht die Desktop-
Anwendung regelmäßig eine UDP-Discovery-Antwort auf Port 2021, damit der
Drucker im lokalen Netzwerk beziehungsweise in Bambu Studio gefunden werden
kann.

Der Code wurde mit einem Bambu Lab P1S getestet. Andere Druckermodelle sind
nicht Bestandteil der bisherigen Tests.

Dieses Projekt ist eine unabhängige, nicht von Bambu Lab entwickelte Software.
Bambu Lab, Bambu Studio und P1S sind Marken ihrer jeweiligen Inhaber. Das
Projekt wird von Bambu Lab weder bereitgestellt noch offiziell unterstützt.

### Aktueller Status der Seriennummer

Die Drucker-Seriennummer wird derzeit noch manuell in
`DESKTOP/bambuproxy.h` hinterlegt. Das ist ein Übergangszustand und wird in
der nächsten Variante bzw. im ersten Release-Paket über das Tray-Menü oder
ähnliche Konfigurationsdialoge automatisch ermittelt und gesetzt werden.

## Lokaler Betrieb ohne Cloud

Mit der Bridge kann der Drucker an den Bambu-Lab-Cloud-Diensten vorbei im
lokalen Netzwerk betrieben werden. Für diesen Betrieb ist kein Bambu-Lab-
Account erforderlich.

Vor der Verbindung des Druckers mit dem WLAN-Access-Point des Arduino UNO R4
WiFi sollte der Drucker in seinen Netzwerkeinstellungen auf **Nur LAN** und
in den Entwicklereinstellungen auf **Dev Mode** gestellt werden. Erst danach
sollte die Verbindung mit dem vom Arduino bereitgestellten WLAN hergestellt
werden.

### Drucker über `config.txt` einrichten

Die WLAN-Verbindung des Druckers wird über eine `config.txt` auf dem externen
Speicher des Druckers vorbereitet:

1. Den externen Speicher des Druckers am Computer anschließen.
2. Im Hauptverzeichnis des Speichers eine Datei mit dem Namen `config.txt`
  anlegen oder die vorhandene Datei öffnen.
3. In der Datei die vom Drucker erwarteten WLAN-Einträge für den Access Point
  hinterlegen. Die Werte dieser Firmware sind:

  - SSID: `KTC_BBL-WSERIAL`
  - Passwort: `BBLWSERIAL`
  - Access-Point-Adresse des Arduino: `192.168.4.1`
  - Erwartete Druckeradresse: `192.168.4.2`

  Die genaue Schreibweise der Schlüssel und Zeilen in `config.txt` hängt von
  der Drucker-Firmware ab und ist nicht Teil dieses Repositories. Dafür muss
  die zur Firmware passende Bambu-Dokumentation verwendet werden. Nach dem
  Einlegen des Speichers und dem Neustart des Druckers sollte zunächst geprüft
  werden, ob der Drucker im Modus **Nur LAN** und **Dev Mode** läuft. Danach
  kann er mit dem WLAN des Arduino verbunden werden.

## Netzwerkdienste und Sicherheit

Die Bridge verwendet folgende feste Netzwerkwerte:

| Dienst | Port | Zweck |
| --- | ---: | --- |
| MQTT-Proxy | `8883` | Weiterleitung der MQTT-Kommunikation |
| FTP-Steuerkanal | `990` | Weiterleitung der FTPS-Steuerverbindung |
| FTP-Datenkanal | `2024` | Weiterleitung der FTP-Nutzdaten |
| UDP-Discovery | `2021` | Erkennung durch Bambu Studio |

Der Arduino stellt den Access Point `KTC_BBL-WSERIAL` mit dem Passwort
`BBLWSERIAL` bereit. Diese Zugangsdaten sind absichtlich fest in
`MCU/src/main.cpp` hinterlegt und werden öffentlich mit dem Projekt verteilt.
Wer eigene Zugangsdaten verwenden möchte, muss SSID und Passwort dort ändern,
die Firmware neu bauen und anschließend auf den Arduino flashen. Die
Netzwerkadressen `192.168.4.1` für den Arduino und `192.168.4.2` für den
Drucker sind ebenfalls Bestandteil der aktuellen Firmware.

Die Desktop-Anwendung bindet die Proxy-Dienste standardmäßig nur an eine
explizit gewählte lokale Adresse. Das ist der sichere Default für einen
vertrauenswürdigen lokalen Rechner oder ein lokales Heimnetz. Eine Bindung auf
`0.0.0.0` beziehungsweise `QHostAddress::Any` ist für diese Software nicht der
Standardpfad. Die Proxy-Dienste besitzen keine zusätzliche
Benutzerverwaltung. Das WLAN und die Proxy-Ports sollten deshalb nur in einem
vertrauten lokalen Netzwerk verwendet und nicht ins Internet oder in ein
unkontrolliertes WLAN weitergeleitet werden.

## Bambu Studio

Eine Verwendung mit Bambu Studio ist möglich, aber derzeit nicht in allen
Funktionen fehlerfrei. Im bisherigen Test mit einem Bambu Lab P1S ergab sich
folgendes Bild:

- Kalibrierungsprogramme lassen sich vollständig und zuverlässig starten.
- Steuerungen über MQTT-Befehle werden zuverlässig an den Drucker übertragen.
- Uploads und das direkte Starten aus dem Slicer sind derzeit nicht stabil.
- Andere Funktionen von Bambu Studio können weiterhin Einschränkungen oder
  Fehlverhalten zeigen.

Beim Upload und Start kann es mehrere Versuche benötigen, bis der Upload
tatsächlich beginnt. Sobald er gestartet wurde, funktioniert der weitere
Ablauf in der Regel. Bis dahin kann Bambu Studio mehrfach nach der Verbindung
fragen.

Wird im Verbindungsfenster keine lokale Netzwerkadresse, zum Beispiel
`192.168.1.200`, angezeigt, kann das Fenster einfach geschlossen werden. Wird
die lokale Adresse angezeigt, sollte die Verbindung bei Bedarf erneut
hergestellt werden, bis der grüne Bestätigungstext erscheint. Anschließend
muss in Bambu Studio der Vorgang **Upload & Start** beziehungsweise **Senden**
erneut gestartet werden.

## Alternativen zu Bambu Studio

Für FTP-Uploads kann [WinSCP](https://winscp.net/) verwendet werden. Die
FTP-Uploads liefen im bisherigen Test stabil. Für die Verbindung muss eine
FTP-Verbindung mit implizitem TLS beziehungsweise FTPS verwendet werden. Die
entsprechenden Einstellungen sind in der [WinSCP-Dokumentation zu FTP und
FTPS](https://winscp.net/eng/docs/ftps) beschrieben.

Für MQTT kann [MQTT Explorer](https://mqtt-explorer.com/) eingesetzt werden.
Die MQTT-Kommunikation lief damit im bisherigen Test ebenfalls stabil. Die
[MQTT-Explorer-Dokumentation und das Wiki](https://github.com/thomasnordquist/MQTT-Explorer/wiki)
enthalten Hinweise zur Einrichtung und zur Verbindung mit einem MQTT-Broker.
Die für die Druckerkommunikation relevanten MQTT-Strukturen sind zusätzlich
im Projekt [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI/tree/main)
dokumentiert.

## Projektaufbau

| Verzeichnis | Inhalt |
| --- | --- |
| `DESKTOP/` | Qt-Anwendung mit serieller Bridge, TCP-Proxy und UDP-Discovery |
| `MCU/` | PlatformIO-Projekt für den Arduino UNO R4 WiFi |

## Entwicklungsumgebungen und offizielle IDEs

Für die Entwicklung, das Bauen und den Upload können die offiziellen Tools der
jeweiligen Ökosysteme verwendet werden:

- [Qt Creator](https://doc.qt.io/qtcreator/creator-overview.html) für die
  Desktop-Anwendung
- [Arduino IDE](https://support.arduino.cc/hc/en-us/articles/360019833020-Install-the-Arduino-IDE)
  für die Arduino- und Board-Konfiguration
- [PlatformIO IDE](https://docs.platformio.org/en/latest/core/installation.html)
  oder [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
  für den MCU-Build und Upload
- [Bambu Lab Wiki](https://wiki.bambulab.com/) für Hardware-, Firmware- und
  Netzwerkinformationen des Druckers

## Voraussetzungen

- Qt 6.11.1 mit den Modulen `Core`, `SerialPort`, `Network` und `Widgets`
- Ein C++17-fähiger Compiler
- PlatformIO Core oder Visual Studio Code mit der PlatformIO-Erweiterung
- Arduino UNO R4 WiFi
- USB-Verbindung zwischen Desktop-Rechner und MCU

## Desktop-Anwendung mit Qt kompilieren

Das Qt-Projekt liegt in `DESKTOP/bbl_SerialBridge.pro`. Es verwendet C++17
und die Qt-Module `Core`, `SerialPort`, `Network` und `Widgets`.

### Mit Qt Creator

1. `DESKTOP/bbl_SerialBridge.pro` in Qt Creator öffnen.
2. Als Kit Qt 6.11.1 und einen passenden Desktop-Compiler auswählen.
3. Das Projekt konfigurieren.
4. Das Projekt bauen und anschließend ausführen.

### Mit der Kommandozeile

Im Repository-Stammverzeichnis:

```bash
cd DESKTOP
qmake6 bbl_SerialBridge.pro
make -j"$(nproc)"
```

Je nach Qt-Installation kann der Befehl auch `qmake` statt `qmake6` heißen.
Das erzeugte Programm wird anschließend aus dem von qmake verwendeten Build-
Verzeichnis gestartet.

## MCU-Firmware mit PlatformIO bauen

Die PlatformIO-Konfiguration liegt in `MCU/platformio.ini`. Die dort
definierte Umgebung heißt `uno_r4_wifi` und verwendet:

- Plattform `renesas-ra`,
- Board `uno_r4_wifi`,
- Arduino-Framework.

Im Repository-Stammverzeichnis:

```bash
cd MCU
pio run -e uno_r4_wifi
```

Mit der PlatformIO-Erweiterung kann alternativ das Projektverzeichnis `MCU/`
geöffnet und die Umgebung `uno_r4_wifi` ausgewählt werden. Der Build wird dann
über die Build-Funktion der Erweiterung gestartet.

## Firmware auf den MCU flashen

1. Arduino UNO R4 WiFi per USB anschließen.
2. Das Board und den seriellen Port des Boards ermitteln.
3. Im Verzeichnis `MCU/` den Upload starten:

```bash
pio run -e uno_r4_wifi -t upload --upload-port /dev/ttyACM0
```

`/dev/ttyACM0` muss durch den tatsächlich verwendeten Port ersetzt werden.
Unter Windows wird stattdessen beispielsweise `COM5` verwendet.

Nach dem Flashen startet die Firmware den Access Point mit der im Quellcode
festgelegten Netzwerkadresse. Für Diagnosezwecke kann der serielle Monitor
gestartet werden:

```bash
pio device monitor --port /dev/ttyACM0 --baud 25600
```

Die Monitor-Baudrate von 25600 stammt aus `platformio.ini`. Die Firmware
initialisiert ihre Bridge-Kommunikation im Quellcode jedoch mit 128000 Baud.
Wenn die Ausgabe des Monitors unleserlich ist, sollte der Monitor daher mit
`--baud 128000` gestartet werden. Für die eigentliche Kommunikation müssen
Desktop-Anwendung und MCU dieselben seriellen Einstellungen verwenden.

## Mitmachen

Beiträge zu Code, Firmware und Dokumentation sind willkommen. Bevor größere
Änderungen begonnen werden, sollte zunächst ein Issue eröffnet oder ein
bestehendes Issue ergänzt werden. So lässt sich klären, ob das Problem bereits
bekannt ist und welche Lösung zum Aufbau der Bridge passt.

Für einen Beitrag empfiehlt sich folgender Ablauf:

1. Repository forken und für die Änderung einen eigenen Branch anlegen.
2. Die Änderung möglichst klein und auf ein Thema begrenzt halten.
3. Bei Änderungen an der Desktop-Anwendung den Qt-Build prüfen.
4. Bei Änderungen an der Firmware den PlatformIO-Build prüfen.
5. Änderungen an der Hardwarekommunikation nach Möglichkeit mit einem
  Arduino UNO R4 WiFi und einem Bambu Lab P1S testen.
6. Testergebnis, verwendete Hardware, Druckermodus und gegebenenfalls den
  seriellen Port im Pull Request dokumentieren.
7. Einen Pull Request mit einer kurzen Beschreibung der Änderung und ihrer
  Auswirkungen erstellen.

Besonders hilfreich sind reproduzierbare Fehlerbeschreibungen, Protokoll- oder
MQTT-Beispiele, Verbesserungen der README sowie Hinweise zu weiteren
Druckermodellen. Zugangsdaten, private Netzwerkadressen und andere vertrauliche
Daten dürfen nicht in Issues, Pull Requests oder den Quellcode übernommen
werden.

## Urheberrecht und Drittanbieter

Der Quellcode dieses Projekts steht unter der MIT-Lizenz in [LICENSE](LICENSE).
Die Grafiken `DESKTOP/Logo.png`, `DESKTOP/Logo.jpg` und
`DESKTOP/appicon.ico` wurden vom Projektautor mit NanoBanana erstellt und
stehen unter dessen Urheberrecht. Sie sind nicht Bestandteil einer Bambu-Lab-
Marke oder offizieller Bambu-Lab-Grafiken.

Für die verwendeten Werkzeuge und Bibliotheken gelten zusätzlich deren eigene
Lizenzbedingungen. Vor der Weitergabe fertiger Binärpakete sollten die jeweils
mitgelieferten Lizenztexte und Qt-Hinweise geprüft werden:

- [Qt Open-Source-Lizenzen und LGPL-Hinweise](https://www.qt.io/development/open-source-lgpl-obligations)
- [Qt-Liste der verwendeten Drittanbieter-Lizenzen](https://doc.qt.io/qt-6/licenses-used-in-qt.html)
- [ArduinoCore-renesas](https://github.com/arduino/ArduinoCore-renesas)
- [WiFiS3](https://github.com/arduino-libraries/WiFiS3)
- [Arduino LED Matrix](https://github.com/arduino-libraries/Arduino_LED_Matrix)
- [PlatformIO-Lizenz und Quellcode](https://github.com/platformio/platformio-core)
- [OpenBambuAPI-Lizenz](https://github.com/Doridian/OpenBambuAPI/blob/main/LICENSE.md)

Die genannten Projekte werden nur verwendet beziehungsweise referenziert und
sind keine Bestandteile dieses Projekts. Falls Bibliotheken oder Quelltexte in
ein Release-Archiv kopiert werden, müssen deren vollständige Lizenz- und
Copyright-Hinweise zusätzlich mit diesem Archiv ausgeliefert werden.

## Quellen und weiterführende Dokumentation

- [Bambu Lab Wiki](https://wiki.bambulab.com/)
- [Arduino Dokumentation](https://docs.arduino.cc/)
- [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI/tree/main)
- [WinSCP-Dokumentation](https://winscp.net/eng/docs/start)
- [WinSCP-Dokumentation zu FTP und FTPS](https://winscp.net/eng/docs/ftps)
- [MQTT Explorer](https://mqtt-explorer.com/)
- [MQTT Explorer Wiki](https://github.com/thomasnordquist/MQTT-Explorer/wiki)
- [C++ Referenz](https://en.cppreference.com/w/)
- [Qt 6 Dokumentation](https://doc.qt.io/qt-6/)
