QT       += core gui widgets

CONFIG   += c++11

TARGET = OpenCV_CameraMonitor
TEMPLATE = app

INCLUDEPATH += /usr/local/arm_opencv480/include/opencv4

LIBS += -L/usr/local/arm_opencv480/lib \
        -l:libopencv_core.so.4.8.0 \
        -l:libopencv_highgui.so.4.8.0 \
        -l:libopencv_imgproc.so.4.8.0 \
        -l:libopencv_videoio.so.4.8.0 \
        -l:libopencv_imgcodecs.so.4.8.0 \
        -l:libopencv_dnn.so.4.8.0

QMAKE_LFLAGS += -Wl,-rpath=/usr/local/arm_opencv480/lib

SOURCES += main.cpp\
           i2c_master.cpp \
           inference.cpp \
           mainwindow.cpp

HEADERS  += mainwindow.h\
            i2c_master.h \
            inference.h

DEFINES += QT_DEPRECATED_WARNINGS
