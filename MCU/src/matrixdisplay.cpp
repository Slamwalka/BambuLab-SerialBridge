#include "matrixdisplay.h"

const uint8_t MatrixDisplay::ICON_WAIT[8][12] = {
    {0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,0,0,0,0,0,0,1,0,0}, {0,0,1,0,0,0,0,0,0,1,0,0},
    {0,0,1,0,0,0,0,0,0,1,0,0}, {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0}
};

const uint8_t MatrixDisplay::ICON_OK[8][12] = {
    {0,0,0,0,0,0,0,0,0,0,0,1}, {0,0,0,0,0,0,0,0,0,0,1,1},
    {0,0,0,0,0,0,0,0,0,1,1,0}, {0,0,0,0,0,0,0,0,1,1,0,0},
    {1,1,0,0,0,0,0,1,1,0,0,0}, {0,1,1,0,0,0,1,1,0,0,0,0},
    {0,0,1,1,0,1,1,0,0,0,0,0}, {0,0,0,1,1,1,0,0,0,0,0,0}
};

MatrixDisplay::MatrixDisplay() {}

void MatrixDisplay::begin() { _matrix.begin(); }

void MatrixDisplay::update(bool connected, bool dataReceived) {
    const uint8_t (*icon)[12] = connected ? ICON_OK : ICON_WAIT;
    
    for(int r = 0; r < 8; r++) {
        for(int c = 0; c < 12; c++) {
            _frame[r][c] = icon[r][c];
        }
    }
    
    if (dataReceived) _frame[0][0] = 1; // Markiert aktuelle Datenaktivität.
    _matrix.renderBitmap(_frame, 8, 12);
}