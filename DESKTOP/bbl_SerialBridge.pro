QT = core serialport network widgets

CONFIG += c++17

# Diese Option kann veraltete Qt-APIs beim Kompilieren als Fehler behandeln.
# Die Einstellung bleibt standardmäßig deaktiviert.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # Qt-APIs vor Qt 6.0.0 deaktivieren

SOURCES += \
        bambuproxy.cpp \
        main.cpp \
        serialbridge.cpp

# Standardpfade für die Installation des Ziels.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    bambuproxy.h \
    serialbridge.h

RESOURCES += \
    res.qrc

RC_ICONS = appicon.ico
