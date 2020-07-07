#ifndef _NEXTGENLOGOGEDETECTOR2_H_
#define _NEXTGENLOGOGEDETECTOR2_H_

#include "Classic/LogoDetectorBase.h"

extern "C" {
#include "libavcodec/avcodec.h"    /* AVPicture */
}
#include "Detector2/FrameAnalyzer.h"

class PGMConverter;
class BorderDetector;
class EdgeDetector;
class CannyEdgeDetector;
class CAVPicture;
class NextgenCommDetector;

struct Point
{
    Point() : x(0), y(0) { }
    int x;
    int y;
};
struct Rect
{
    Point   tl;
    Point   br;
};

class Component : QSharedData
{
    public:
        Component() : area(0), maskStabilityCount(0), cTemplate(NULL), cMask(NULL), index(-1)
        {
        }
        Component(int x, int y) : area(0), maskStabilityCount(0), cTemplate(NULL), cMask(NULL), index(-1)
        {
            boundingBox.tl.x = boundingBox.br.x = x;
            boundingBox.tl.y = boundingBox.br.y = y;
        }
        ~Component();

        int             area;
        int             maskStabilityCount;
        Rect            boundingBox;
        CAVPicture*     cTemplate;
        CAVPicture*     cMask;
        int             index;  // index of subframe
        friend class QExplicitlySharedDataPointer< Component >;
};
typedef QExplicitlySharedDataPointer< Component > PComponent;
typedef list<PComponent> ComponentList;

class NextgenLogoDetector2 : public LogoDetectorBase
{
  public:
    /* Ctor/dtor. */
    NextgenLogoDetector2(NextgenCommDetector* commDetector,unsigned int width,
        unsigned int height, unsigned int commdetectborder,
        unsigned int xspacing, unsigned int yspacing);
    virtual void deleteLater(void);
    //NextgenLogoDetector2(PGMConverter *pgmc, BorderDetector *bd, EdgeDetector *ed,
            //MythPlayer *player, int proglen, QString debugdir);

    bool searchForLogo(MythPlayer* player);
    bool doesThisFrameContainTheFoundLogo(VideoFrame* frame);
    bool pixelInsideLogo(unsigned int x, unsigned int y);

    unsigned int getRequiredAvailableBufferForSearch();

  protected:
    virtual ~NextgenLogoDetector2() {}

  private:
    NextgenCommDetector* commDetector;

    PGMConverter    *pgmConverter;
    BorderDetector  *borderDetector;
    EdgeDetector    *edgeDetector;

    CAVPicture      *averageImage;
    CAVPicture      *avgSubImage[4];
    CAVPicture      *previousEdges[4];
    CAVPicture      *timeAveragedEdges[4];
    CAVPicture      *timeAveragedThresholdedEdges[4];
    CAVPicture      *previousCalcImg[4];
    ComponentList   logoList;

    int             persistenceCount[4];
    int             subWidth;
    int             subHeight;

    // algo parameters
    int commDetectLogoSamplesNeeded;
    int commDetectLogoSampleSpacing;
    int commDetectLogoSampleAveraging;
    int commDetectLogoSecondsNeeded;

    int commDetectLogoSampleSpacingInFrames;
};

#endif

