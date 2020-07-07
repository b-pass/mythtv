include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += widgets

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)



DEPENDPATH  += .
INCLUDEPATH += .

HEADERS += commandlineparser.h
HEADERS += SlotRelayer.h
HEADERS += CustomEventRelayer.h
HEADERS += CommDetectorFactory.h
HEADERS += CommDetectorBase.h

SOURCES += main.cpp
SOURCES += commandlineparser.cpp
SOURCES += CommDetectorFactory.cpp
SOURCES += CommDetectorBase.cpp

SOURCES += ../../libs/libmythtv/subtitlereader.cpp
SOURCES += ../../libs/libmythtv/textsubtitleparser.cpp
SOURCES += ../../libs/libmythtv/xine_demux_sputext.cpp
SOURCES += ../../libs/libmythtv/teletextreader.cpp
SOURCES += ../../libs/libmythtv/vbilut.cpp
SOURCES += ../../libs/libmythtv/filtermanager.cpp



DEPENDPATH  += ./Classic
HEADERS += ./Classic/ClassicCommDetector.h
HEADERS += ./Classic/ClassicLogoDetector.h
HEADERS += ./Classic/ClassicSceneChangeDetector.h
HEADERS += ./Classic/Histogram.h
HEADERS += ./Classic/LogoDetectorBase.h
HEADERS += ./Classic/SceneChangeDetectorBase.h

SOURCES += ./Classic/ClassicCommDetector.cpp
SOURCES += ./Classic/ClassicLogoDetector.cpp
SOURCES += ./Classic/ClassicSceneChangeDetector.cpp
SOURCES += ./Classic/Histogram.cpp



DEPENDPATH  += ./Detector2
HEADERS += ./Detector2/CommDetector2.h
HEADERS += ./Detector2/quickselect.h
HEADERS += ./Detector2/pgm.h
HEADERS += ./Detector2/EdgeDetector.h ./Detector2/CannyEdgeDetector.h
HEADERS += ./Detector2/PGMConverter.h ./Detector2/BorderDetector.h
HEADERS += ./Detector2/FrameAnalyzer.h
HEADERS += ./Detector2/TemplateFinder.h ./Detector2/TemplateMatcher.h
HEADERS += ./Detector2/HistogramAnalyzer.h
HEADERS += ./Detector2/BlankFrameDetector.h
HEADERS += ./Detector2/SceneChangeDetector.h

SOURCES += ./Detector2/CommDetector2.cpp
SOURCES += ./Detector2/quickselect.c
SOURCES += ./Detector2/pgm.cpp
SOURCES += ./Detector2/EdgeDetector.cpp ./Detector2/CannyEdgeDetector.cpp
SOURCES += ./Detector2/PGMConverter.cpp ./Detector2/BorderDetector.cpp
SOURCES += ./Detector2/FrameAnalyzer.cpp
SOURCES += ./Detector2/TemplateFinder.cpp ./Detector2/TemplateMatcher.cpp
SOURCES += ./Detector2/HistogramAnalyzer.cpp
SOURCES += ./Detector2/BlankFrameDetector.cpp
SOURCES += ./Detector2/SceneChangeDetector.cpp



DEPENDPATH += ./PrePostRoll
HEADERS += ./PrePostRoll/PrePostRollFlagger.h
SOURCES += ./PrePostRoll/PrePostRollFlagger.cpp



DEPENDPATH  += ./NextGen
HEADERS += ./NextGen/NextgenCommDetector.h
HEADERS += ./NextGen/NextgenLogoDetector.h
HEADERS += ./NextGen/NextgenLogoDetector2.h
HEADERS += ./NextGen/NextgenSceneChangeDetector.h
HEADERS += ./NextGen/AudioChangeDetector.h ./NextGen/AudioBuffer.h
HEADERS += ./NextGen/SubtitleChangeDetector.h
HEADERS += ./NextGen/AudioChangeDetectorBase.h
HEADERS += ./NextGen/SubtitleChangeDetectorBase.h

SOURCES += ./NextGen/NextgenCommDetector.cpp
SOURCES += ./NextGen/AudioChangeDetectorBase.cpp
SOURCES += ./NextGen/SubtitleChangeDetectorBase.cpp
SOURCES += ./NextGen/AudioChangeDetector.cpp ./NextGen/AudioBuffer.cpp
SOURCES += ./NextGen/SubtitleChangeDetector.cpp
SOURCES += ./NextGen/NextgenLogoDetector.cpp
SOURCES += ./NextGen/NextgenLogoDetector2.cpp
SOURCES += ./NextGen/NextgenSceneChangeDetector.cpp



DEPENDPATH  += ./Detector3
HEADERS += ./Detector3/CommDetector3.h
HEADERS += ./Detector3/FrameMetadata.h
HEADERS += ./Detector3/FrameMetadataAggregator.h
HEADERS += ./Detector3/LogoDetector.h
HEADERS += ./Detector3/Sobel.hpp
HEADERS += ./Detector3/BlankDetector.h
HEADERS += ./Detector3/SceneDetector.h
HEADERS += ./Detector3/Deinterlacer.h
HEADERS += ./Detector3/AudioDetector.h

SOURCES += ./Detector3/CommDetector3.cpp
SOURCES += ./Detector3/FrameMetadataAggregator.cpp
SOURCES += ./Detector3/LogoDetector.cpp
SOURCES += ./Detector3/BlankDetector.cpp
SOURCES += ./Detector3/SceneDetector.cpp
SOURCES += ./Detector3/Deinterlacer.cpp
SOURCES += ./Detector3/AudioDetector.cpp

QT += xml sql network

