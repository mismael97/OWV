QT       += core gui widgets

CONFIG += c++17

TARGET = VCDWaveViewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    vcdparser.cpp \
    waveformwidget.cpp

HEADERS += \
    mainwindow.h \
    vcdparser.h \
    waveformwidget.h

win32: LIBS += -luser32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
