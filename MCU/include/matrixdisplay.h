#pragma once

#include "Arduino_LED_Matrix.h"

/**
 * @brief Zeigt Verbindungs- und Aktivitätszustände auf der LED-Matrix an.
 */
class MatrixDisplay {
public:
    /**
     * @brief Erzeugt die Anzeige mit den vorbereiteten Statussymbolen.
     */
    MatrixDisplay();

    /**
     * @brief Initialisiert die LED-Matrix.
     */
    void begin();

    /**
     * @brief Wählt das Statussymbol und zeichnet es auf der Matrix.
     * @param connected Gibt an, ob eine Druckerverbindung besteht.
     * @param dataReceived Markiert, ob aktuell Daten empfangen wurden.
     */
    void update(bool connected, bool dataReceived);

private:
    ArduinoLEDMatrix _matrix;
    uint8_t _frame[8][12];
    
    static const uint8_t ICON_WAIT[8][12];
    static const uint8_t ICON_OK[8][12];
};
