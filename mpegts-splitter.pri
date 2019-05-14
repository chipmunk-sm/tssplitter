
HEADERS += ./bitstream.h \
    ./ts_aac.h \
    ./ts_ac3.h \
    ./ts_h264.h \
    ./ts_mpegaudio.h \
    ./ts_mpegvideo.h \
    ./ts_subtitle.h \
    ./ts_teletext.h \
    ./tsstream.h \
    ./tspackage.h \
    ./tscontext.h \
    ./tstable.h \
    ./tsparser.h \
    ./mainwindow.h

SOURCES += ./bitstream.cpp \
    ./main.cpp \
    ./mainwindow.cpp \
    ./ts_aac.cpp \
    ./ts_ac3.cpp \
    ./ts_h264.cpp \
    ./ts_mpegaudio.cpp \
    ./ts_mpegvideo.cpp \
    ./ts_subtitle.cpp \
    ./ts_teletext.cpp \
    ./tsparser.cpp \
    ./tsstream.cpp \
    ./tscontext.cpp

RESOURCES += \
    ./mpegts.qrc
