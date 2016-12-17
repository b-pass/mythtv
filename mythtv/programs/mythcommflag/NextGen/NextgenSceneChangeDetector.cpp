#include <algorithm>
using namespace std;

// MythTV headers
#include "mythcontext.h"
#include "mythframe.h"

#include "NextgenSceneChangeDetector.h"
#include "Classic/Histogram.h"

NextgenSceneChangeDetector::NextgenSceneChangeDetector(unsigned int width,
        unsigned int height, unsigned int commdetectborder_in,
        unsigned int xspacing_in, unsigned int yspacing_in):
    SceneChangeDetectorBase(width,height),
    frameNumber(0),
    previousFrameWasSceneChange(false),
    xspacing(xspacing_in),
    yspacing(yspacing_in),
    commdetectborder(commdetectborder_in)
{
    histogram = new Histogram;
    previousHistogram = new Histogram;
    commDetectSceneChangeThreshold =
        gCoreContext->GetNumSetting("CommDetectSceneChangeThreshold", 70)/100.0;
}

void NextgenSceneChangeDetector::deleteLater(void)
{
    delete histogram;
    delete previousHistogram;
    SceneChangeDetectorBase::deleteLater();
}

void NextgenSceneChangeDetector::processFrame(VideoFrame* frame)
{
    int width = frame->pitches[0];
    histogram->generateFromImage(frame, width, height, commdetectborder,
                                 width-commdetectborder, commdetectborder,
                                 height-commdetectborder, xspacing, yspacing);
    float similar = histogram->calculateSimilarityWith(*previousHistogram);

    bool isSceneChange = (similar < commDetectSceneChangeThreshold && !previousFrameWasSceneChange);

    emit(haveNewInformation(frameNumber,isSceneChange,similar));
    previousFrameWasSceneChange = isSceneChange;

    std::swap(histogram,previousHistogram);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

