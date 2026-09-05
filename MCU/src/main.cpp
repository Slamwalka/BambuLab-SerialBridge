#include "printerbridge.h"
#include "serialprotocol.h"
#include "matrixdisplay.h"

PrinterBridge bridge(IPAddress(192, 168, 4, 2));
SerialProtocol parser;
MatrixDisplay display;
unsigned long lastDataTime = 0;

const char *ssid = "KTC_BBL-WSERIAL";
const char *pass = "BBLWSERIAL";
IPAddress apIP(192, 168, 4, 1);

void setupWiFi()
{
  // Konfiguriert die feste Adresse des Access Points.
  WiFi.config(apIP);

  // Startet den Access Point und bleibt bei einem Startfehler in der Schleife.
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING)
  {
    while (true)
    {
      delay(500);
    }
  }
}

void setup()
{
  Serial.begin(128000);

  display.begin();

  setupWiFi();
}

void loop()
{
  // Serielle Frames werden vor Netzwerk- und Anzeigeaufgaben verarbeitet.
  while (Serial.available())
  {
    if (parser.feed(Serial.read()))
    {
      lastDataTime = millis();
      bridge.processPacket(parser.getChannel(), parser.getPayload(), parser.getLength());
    }
  }

  // Druckerdaten werden anschließend in Richtung Desktop weitergeleitet.
  bridge.forwardPrinterDataToSerial();

  // Die Matrix wird höchstens alle 100 Millisekunden aktualisiert.
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100)
  {
    display.update(bridge.isAnyConnected(), (millis() - lastDataTime < 150));
    lastUpdate = millis();
  }

}