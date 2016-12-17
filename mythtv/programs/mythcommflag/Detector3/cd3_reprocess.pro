include (../../../version.pro)
include (../../../settings.pro)

INCLUDEPATH += ../../../libs/libmythbase ../../../libs/libmyth ../../../libs/libmythtv ../../../external/FFmpeg
LIBS += -L../../../libs/libmythbase -L../../../libs/libmyth
LIBS += -lmythbase-$$LIBVERSION -lmyth-$$LIBVERSION
POST_TARGETDEPS += ../../../libs/libmythbase/libmythbase-$${MYTH_SHLIB_EXT} ../../../libs/libmyth/libmyth-$${MYTH_SHLIB_EXT}

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

INCLUDEPATH += .

HEADERS += FrameMetadata.h
HEADERS += FrameMetadataAggregator.h

SOURCES += FrameMetadataAggregator.cpp
SOURCES += Reprocess.cpp



#The following line was inserted by qt3to4
QT += xml sql network

