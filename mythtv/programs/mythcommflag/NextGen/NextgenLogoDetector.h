#ifndef _NEXTGENLOGOGEDETECTOR_H_
#define _NEXTGENLOGOGEDETECTOR_H_

#include "Classic/LogoDetectorBase.h"

typedef struct edgemaskentry EdgeMaskEntry;
typedef struct VideoFrame_ VideoFrame;
class NextgenCommDetector;

class NextgenLogoDetector : public LogoDetectorBase
{
  public:
    NextgenLogoDetector(NextgenCommDetector* commDetector,unsigned int width,
        unsigned int height, unsigned int commdetectborder,
        unsigned int xspacing, unsigned int yspacing);
    virtual void deleteLater(void);

    bool searchForLogo(MythPlayer* player);
    bool doesThisFrameContainTheFoundLogo(VideoFrame* frame);
    bool pixelInsideLogo(unsigned int x, unsigned int y);

    unsigned int getRequiredAvailableBufferForSearch();

  protected:
    virtual ~NextgenLogoDetector() {}

  private:
    void SetLogoMaskArea();
    //void SetLogoMask(unsigned char *mask);
    //void DumpLogo(bool fromCurrentFrame,unsigned char* framePtr);
    void DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges, int edgeDiff);

    NextgenCommDetector* commDetector;
    bool previousFrameWasSceneChange;
    unsigned int xspacing, yspacing;
    unsigned int commDetectBorder;

    int commDetectLogoSamplesNeeded;
    int commDetectLogoSampleSpacing;
    int commDetectLogoSecondsNeeded;
    double commDetectLogoGoodEdgeThreshold;
    double commDetectLogoBadEdgeThreshold;

    EdgeMaskEntry *edgeMask;

    unsigned char *logoMaxValues;
    unsigned char *logoMinValues;
    unsigned char *logoFrame;
    unsigned char *logoMask;
    unsigned char *logoCheckMask;
    unsigned char *tmpBuf;

    int logoEdgeDiff;
    unsigned int logoFrameCount;
    unsigned int logoMinX;
    unsigned int logoMaxX;
    unsigned int logoMinY;
    unsigned int logoMaxY;

    bool logoInfoAvailable;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

