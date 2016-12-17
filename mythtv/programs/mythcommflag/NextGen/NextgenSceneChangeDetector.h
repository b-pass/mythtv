#ifndef _NEXTGENSCENECHANGEDETECTOR_H_
#define _NEXTGENSCENECHANGEDETECTOR_H_

#include "Classic/SceneChangeDetectorBase.h"

class Histogram;

class NextgenSceneChangeDetector : public SceneChangeDetectorBase
{
  public:
    NextgenSceneChangeDetector(unsigned int width, unsigned int height,
        unsigned int commdetectborder, unsigned int xspacing,
        unsigned int yspacing);
    virtual void deleteLater(void);

    void processFrame(VideoFrame* frame);

  private:
    ~NextgenSceneChangeDetector() {}

  private:
    Histogram* histogram;
    Histogram* previousHistogram;
    unsigned int frameNumber;
    bool previousFrameWasSceneChange;
    unsigned int xspacing, yspacing;
    unsigned int commdetectborder;
    double commDetectSceneChangeThreshold;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

