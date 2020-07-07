/*
 * Searches:
 * transparent logo detection
 *
 * Refs:
 * http://www.busim.ee.boun.edu.tr/~sankur/SankurFolder/EUSIPCO2009_Automatic_TV_Logo.pdf
 * http://www.graphicon.ru/proceedings/2011/conference/gc2011erofeev.pdf
 * http://www.junjanes.com/files/research/a-fast-method-for-animated-tv-logo-detection.pdf
 * http://www.nlpr.ia.ac.cn/2007papers/gjhy/gh40.pdf
 * http://avss2012.org/2006papers/gjhy/gh40.pdf
 *
 */


// Qt headers
#include <QSharedPointer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// MythTV headers
#include "mythplayer.h"
#include "mythcorecontext.h"    /* gContext */
#include "mythframe.h"          /* VideoFrame */
#include "mythmiscutil.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"

// Commercial Flagging headers
#include "NextgenLogoDetector2.h"
#include "NextgenCommDetector.h"

#include "Detector2/pgm.h"
#include "Detector2/PGMConverter.h"
#include "Detector2/BorderDetector.h"
#include "Detector2/EdgeDetector.h"
#include "Detector2/CannyEdgeDetector.h"

#define SAFE_DELETE(x) if (x) { delete x; x = NULL; }


class Histogram2;
class CAVPicture //: QSharedData
{
    public:
        CAVPicture() : needsFree(false), width(0), height(0)
        {
            memset(&avpic, 0, sizeof(avpic));
        }
        CAVPicture(int width, int height) : needsFree(false), width(0), height(0)
        {
            memset(&avpic, 0, sizeof(avpic));
            alloc(width, height);
            zero();
        }
        CAVPicture(uint8_t* img, int width, int height) : needsFree(false), width(0), height(0)
        {
            memset(&avpic, 0, sizeof(avpic));
            //alloc(width, height);
            avpicture_fill(&avpic, img, AV_PIX_FMT_GRAY8, width, height);
            this->width = width;
            this->height = height;
        }
        CAVPicture(CAVPicture& pic) : needsFree(false), width(0), height(0)
        {
            memset(&avpic, 0, sizeof(avpic));
            copy(pic);
        }
        CAVPicture(CAVPicture& pic, int x, int y, int w, int h) : needsFree(false), width(0), height(0)
        {
            memset(&avpic, 0, sizeof(avpic));
            copy(pic, x, y, w, h);
        }


        ~CAVPicture()
        {
            free();
        }

        bool resize(int width, int height)
        {
            return alloc(width, height);
        }

        void copy(const AVPicture* img)
        {
            //avpicture_fill(&avpic, img->data[0], PIX_FMT_GRAY8, width, height);
            memcpy(avpic.data[0], img->data[0], width * height);
        }

        void copy(const CAVPicture& pic)
        {
            alloc(pic.width, pic.height);
            memcpy(avpic.data[0], pic.avpic.data[0], width * height);
        }
        void copy(const CAVPicture& pic, int x, int y, int w, int h)
        {
            alloc(w, h);
            // note row (y) comes before col (x) in args. this is not wrong
            pgm_crop(&avpic, &pic.avpic, pic.width, y, x, w, h);
        }


        void zero()
        {
            if (avpic.data[0])
            {
                memset(avpic.data[0], 0, width*height);
            }
        }

        AVPicture avpic;

    private:
        bool needsFree;
        int width;
        int height;
        bool alloc(int newwidth, int newheight)
        {
            if (avpic.data[0])
            {
                if (newwidth == width && newheight == height)
                    return true;
                free();
            }
            if (avpicture_alloc(&avpic, AV_PIX_FMT_GRAY8, newwidth, newheight) != 0)
            {
                return false;
            }
            needsFree = true;
            width = newwidth;
            height = newheight;
            return true;
        }
        void free()
        {
            if (avpic.data[0] && needsFree)
            {
                avpicture_free(&avpic);
                memset(&avpic, 0, sizeof(avpic));
                needsFree = false;
            }
        }
    public:
        int writeJPG(QString prefix, bool force = true)
        {
            const int imgwidth = avpic.linesize[0];
            const int imgheight = height;
            QFileInfo jpgfi(prefix + ".jpg");
            if (!jpgfi.exists() || force)
            {
                QFile pgmfile(prefix + ".pgm");
                if (!pgmfile.exists() || force)
                {
                    QByteArray pfname = pgmfile.fileName().toLocal8Bit();
                    if (pgm_write(avpic.data[0], imgwidth, imgheight,
                                pfname.constData()))
                    {
                        return -1;
                    }
                }

                //QString cmd = QString("convert -quality 50 -resize 192x144 %1 %2")
                QString cmd = QString("convert -quality 50 %1 %2")
                    .arg(pgmfile.fileName()).arg(jpgfi.filePath());
                if (myth_system(cmd) != GENERIC_EXIT_OK)
                    return -1;

#if 0
                if (!pgmfile.remove())
                {
                    LOG(VB_COMMFLAG, LOG_ERR, 
                            QString("TemplateFinder.writeJPG error removing %1 (%2)")
                            .arg(pgmfile.fileName()).arg(strerror(errno)));
                    return -1;
                }
#endif
            }
            return 0;
        }
        void AverageImage(unsigned char* framePtr, int srcWidth, int xs, int xe, int ys, int ye, int alpha );
        void Average(const AVPicture* pic, int alpha );
        int  IntersectionRatio(const AVPicture* pic );
        int  SimilarityRatio(const AVPicture* mask, const AVPicture* pic);
        void Threshold(int32_t minThreshold);

        void Dilate(int xsize, int ysize);
        void Erode(int xsize, int ysize);
        void Close(int xsize, int ysize);
        void Open(int xsize, int ysize);
        void Fill();
        uint32_t CalcArea(uint8_t threshold);
        void Invert();
        void Union(const CAVPicture& pic);
        void Intersection(const CAVPicture& pic);
        void GetComponents(ComponentList& components, CAVPicture* templateImage = NULL);
        void LabelConnectedRegion(PComponent&, uint8_t label, int x, int y);
        void SetTemplate(PComponent component);

    protected:
        void Fill1(int x, int y);

    friend class Histogram2;
};
//typedef QExplicitlySharedDataPointer< CAVPicture > PCAVPicture;

class Histogram2
{
    public:
        Histogram2()
        {
            memset(data, 0, sizeof(data));
        }
        Histogram2(const CAVPicture& pic)
        {
            Process(pic);
        }
        ~Histogram2()
        {
        }

        void Process(const CAVPicture& pic)
        {
            int i;
            memset(data, 0, sizeof(data));
            N = pic.width*pic.height;
            for(i = 0; i < N; i++)
            {
                data[pic.avpic.data[0][i]]++;
            }
        }
        // OTSU threshold calc
        int GetThreshold(int last = 256)
        {
            int i;
            double sum, sumB, wB, wF, varMax;
            int threshold = 0;
            sum = sumB = wB = wF = varMax = 0;
            for(i=0;i<last;i++)
            {
                sum += i * data[i];
            }
            for(i=0;i<last;i++)
            {
                wB += data[i];
                if (wB == 0) continue;
                wF = N - wB;
                if (wF == 0) break;

                sumB += i * data[i];
                double mB = sumB/wB;
                double mF = (sum - sumB)/wF;
                double varBetween = wB * wF * (mB - mF) * (mB - mF);
                if (varBetween > varMax)
                {
                    varMax = varBetween;
                    threshold = i;
                }
            }
            return threshold;
        }

        int N;
        int data[256];
};

void CAVPicture::Dilate(int xsize, int ysize)
{
    int x,y,kx,ky;
    CAVPicture refpic(*this);
    for(ky=-ysize/2; ky <= ysize/2; ky++)
    {
        for(kx=-xsize/2; kx <= xsize/2; kx++)
        {
            for(y=0; y < height; y++)
            {
                int yofs = y * width;
                int kofs = (ky+y) * width + kx;
                for(x=0; x < width; x++)
                {
                    if (((ky+y) >= 0) && ((ky+y) < height) &&
                        ((kx+x) >= 0) && ((kx+x) < width))
                        avpic.data[0][yofs + x] |= refpic.avpic.data[0][kofs + x];
                }
            }
        }
    }
}
void CAVPicture::Erode(int xsize, int ysize)
{
    int x,y,kx,ky;
    CAVPicture refpic(*this);
    for(ky=-ysize/2; ky <= ysize/2; ky++)
    {
        for(kx=-xsize/2; kx <= xsize/2; kx++)
        {
            for(y=0; y < height; y++)
            {
                int yofs = y * width;
                int kofs = (ky+y) * width + kx;
                for(x=0; x < width; x++)
                {
                    if (((kofs + x) >= 0) && ((kofs+x) < (width*height)) &&
                        ((kx+x) >= 0) && ((kx+x) < width))
                        avpic.data[0][yofs + x] &= refpic.avpic.data[0][kofs + x];
                    else
                        avpic.data[0][yofs + x] = 0;
                }
            }
        }
    }
}

void CAVPicture::Close(int xsize, int ysize)
{
    Dilate(xsize, ysize);
    Erode(xsize, ysize);
}

void CAVPicture::Open(int xsize, int ysize)
{
    Erode(xsize, ysize);
    Dilate(xsize, ysize);
}

void CAVPicture::Fill1(int x, int y)
{
    if (avpic.data[0][y*width+x])
        return;
    avpic.data[0][y*width+x] = 255;
    if (x < width-1) Fill1(x+1,y);
    if (y < height-1) Fill1(x,y+1);
    if (x > 0) Fill1(x-1,y);
    if (y > 0) Fill1(x,y-1);
}

void CAVPicture::Fill()
{
    CAVPicture tmp(*this);
    // find a seed point
    tmp.Fill1(0,0);
    tmp.Invert();
    Union(tmp);
}

void CAVPicture::Invert()
{
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    for(; p1 < pEnd; p1++)
    {
        *p1 = 255 - *p1;
    }
}

void CAVPicture::Union(const CAVPicture& pic)
{
    uint8_t *p1 = avpic.data[0];
    uint8_t *p2 = pic.avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    for(; p1 < pEnd; p1++, p2++)
    {
        *p1 = *p1 | *p2;
    }
}

void CAVPicture::Intersection(const CAVPicture& pic)
{
    uint8_t *p1 = avpic.data[0];
    uint8_t *p2 = pic.avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    for(; p1 < pEnd; p1++, p2++)
    {
        *p1 = *p1 & *p2;
    }
}

uint32_t CAVPicture::CalcArea(uint8_t threshold)
{
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    uint32_t area = 0;
    for(; p1 < pEnd; p1++)
    {
        if (*p1 >= threshold)
        {
            area++;
        }
    }
    return area;
}

void CAVPicture::AverageImage(unsigned char* data, int srcWidth, int xs, int ys, int w, int h, int alpha )
{
    int x, y;
    int ye = ys+h;
    uint8_t *p = avpic.data[0];
    int alpha1 = alpha-1;
    int alpha2 = alpha/2;

    for(y=ys; y<ye; y++)
    {
        int yofs = y * srcWidth;
        int xe = xs+w+yofs;
        for(x=xs + yofs; x<xe; x++, p++)
        {
            //avpic->data[0][x] = (framePtr[x] + avpic->data[0][x] * (alpha-1))/alpha;
#if 1
            *p = (data[x] + (*p * alpha1) + alpha2)/alpha;
#else
            *p = data[x];
#endif
        }
    }
}

void CAVPicture::Average(const AVPicture* pic, int alpha )
{
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    uint8_t *p2 = pic->data[0];
    int alpha1 = alpha-1;
    int alpha2 = alpha/2;

    for(; p1 < pEnd; p1++, p2++)
    {
            *p1 = (*p2 + (*p1 * alpha1) + alpha2)/alpha;
    }
}

int CAVPicture::IntersectionRatio(const AVPicture* pic )
{
    int pixCount = 0;
    int sameCount = 0;
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    uint8_t *p2 = pic->data[0];

    for(; p1 < pEnd; p1++, p2++)
    {
        if (*p1 || *p2) pixCount++;
        if (*p1 && *p2) sameCount++;
    }

    if (pixCount == 0) return 0;
    return (sameCount * 100)/pixCount;
}

int CAVPicture::SimilarityRatio(const AVPicture* mask, const AVPicture* pic)
{
    int pixCount = 0;
    int sameCount = 0;
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    uint8_t *p2 = pic->data[0];
    uint8_t *pmask = mask->data[0];

    for(; p1 < pEnd; p1++, p2++, pmask++)
    {
        if (*pmask)
        {
            pixCount++;
            if (*p1 == *p2) sameCount++;
        }
    }

    if (pixCount == 0) return 0;
    return (sameCount * 100)/pixCount;
}

void CAVPicture::Threshold(int32_t minThreshold)
{
    Histogram2 hist(*this);
    int thresh = hist.GetThreshold();
    uint8_t *p1 = avpic.data[0];
    uint8_t *pEnd = p1 + width * height;
    if (thresh < minThreshold)
    {
        thresh = minThreshold;
    }
    if (thresh == 0)
    {
        thresh = 1;
    }
    for(; p1 < pEnd; p1++)
    {
        if (*p1 >= thresh)
            *p1 = 255;
        else
            *p1 = 0;
    }
}

void CAVPicture::LabelConnectedRegion(PComponent& component, uint8_t label, int x, int y)
{
    if ((avpic.data[0][y*width+x]) != 255)
        return;
    avpic.data[0][y*width+x] = label;
    component->area++;
    // grow bounding box
    if (x < component->boundingBox.tl.x) component->boundingBox.tl.x = x;
    if (y < component->boundingBox.tl.y) component->boundingBox.tl.y = y;
    if (x > component->boundingBox.br.x) component->boundingBox.br.x = x;
    if (y > component->boundingBox.br.y) component->boundingBox.br.y = y;

    if (x < width-1) LabelConnectedRegion(component, label, x+1,y);
    if (y < height-1) LabelConnectedRegion(component, label, x,y+1);
    if (x > 0) LabelConnectedRegion(component, label, x-1,y);
    if (y > 0) LabelConnectedRegion(component, label, x,y-1);
}

void CAVPicture::GetComponents(ComponentList& components, CAVPicture* templateImage)
{
    CAVPicture work(*this);
    uint8_t label = 1;
    uint8_t *p1 = work.avpic.data[0];
    uint8_t *pEnd = p1 + work.width * work.height;

    for(; p1 < pEnd; p1++)
    {
        if (*p1 == 255)
        {
            int x = (p1-work.avpic.data[0])%work.width;
            int y = (p1-work.avpic.data[0])/work.width;
            PComponent component(new Component(x,y));
            work.LabelConnectedRegion(component, label, x, y);
            // copy it into cMask
            component->cMask = new CAVPicture(*this,
                    component->boundingBox.tl.x,
                    component->boundingBox.tl.y,
                    component->boundingBox.br.x - component->boundingBox.tl.x + 1,
                    component->boundingBox.br.y - component->boundingBox.tl.y + 1);
            if (templateImage)
            {
                component->cTemplate = new CAVPicture(*templateImage, 
                        component->boundingBox.tl.x, 
                        component->boundingBox.tl.y,
                        component->boundingBox.br.x - component->boundingBox.tl.x + 1,
                        component->boundingBox.br.y - component->boundingBox.tl.y + 1);
                component->cTemplate->Intersection(*(component->cMask));
            }
            components.push_back(component);
            label++;
        }
    }
}

void CAVPicture::SetTemplate(PComponent component)
{
    SAFE_DELETE(component->cTemplate);
    component->cTemplate = new CAVPicture(*this, 
            component->boundingBox.tl.x, 
            component->boundingBox.tl.y,
            component->boundingBox.br.x - component->boundingBox.tl.x + 1,
            component->boundingBox.br.y - component->boundingBox.tl.y + 1);
}

Component::~Component()
{
    SAFE_DELETE(cTemplate);
    SAFE_DELETE(cMask);
}

NextgenLogoDetector2::NextgenLogoDetector2(NextgenCommDetector* commdetector,
                                         unsigned int w, unsigned int h,
                                         unsigned int commdetectborder_in,
                                         unsigned int xspacing_in,
                                         unsigned int yspacing_in)
    : LogoDetectorBase(w,h),
      commDetector(commdetector),
      pgmConverter(new PGMConverter()),
      borderDetector(new BorderDetector()),
      edgeDetector(new CannyEdgeDetector()),
      averageImage(new CAVPicture(w,h)),
      logoList()
{
    commDetectLogoSamplesNeeded =
        gCoreContext->GetNumSetting("CommDetectLogoSamplesNeeded", 240);
    commDetectLogoSampleSpacing =
        gCoreContext->GetNumSetting("CommDetectLogoSampleSpacing", 1);
    commDetectLogoSampleAveraging =
        gCoreContext->GetNumSetting("CommDetectLogoSampleAveraging", 20);
    commDetectLogoSecondsNeeded = (int)(1.3 * commDetectLogoSamplesNeeded *
                                              commDetectLogoSampleSpacing);
    commDetectLogoSampleSpacingInFrames = commDetectLogoSampleSpacing * 25;

    // split image to 3:5:3
    subWidth = (width/11.0)*3;
    subHeight = (height/11.0)*3;
    for(int i = 0; i < 4; i++)
    {
        avgSubImage[i] = new CAVPicture(subWidth, subHeight);
        previousEdges[i] = new CAVPicture(subWidth, subHeight);
        previousCalcImg[i] = new CAVPicture(subWidth, subHeight);
        timeAveragedEdges[i] = new CAVPicture(subWidth, subHeight);
        timeAveragedThresholdedEdges[i] = new CAVPicture(subWidth, subHeight);
        persistenceCount[i] = 0;
    }

}

void NextgenLogoDetector2::deleteLater(void)
{
    commDetector = NULL;

    for(int i = 0; i < 4; i++)
    {
        SAFE_DELETE(avgSubImage[i]);
        SAFE_DELETE(previousEdges[i]);
        SAFE_DELETE(previousCalcImg[i]);
        SAFE_DELETE(timeAveragedEdges[i]);
        SAFE_DELETE(timeAveragedThresholdedEdges[i]);
    }
    SAFE_DELETE(pgmConverter);
    SAFE_DELETE(borderDetector);
    SAFE_DELETE(edgeDetector);

    LogoDetectorBase::deleteLater();
}

unsigned int NextgenLogoDetector2::getRequiredAvailableBufferForSearch()
{
    return commDetectLogoSecondsNeeded;
}

bool NextgenLogoDetector2::searchForLogo(MythPlayer* /*player*/)
{
    // allow rest of algo to work so always true
    return true;
}

bool NextgenLogoDetector2::doesThisFrameContainTheFoundLogo(
    VideoFrame * frame)
{
    const int           FRAMESGMPCTILE = 95;
    int                 i;
    const AVPicture     *edges;
    unsigned char       *framePtr = frame->buf;
    uint32_t            rowWidth = frame->pitches[0];

    //return false;

#define AVG_MODE 0
#if AVG_MODE
#define AVERAGE_COEFF commDetectLogoSampleSpacingInFrames
//#define AVERAGE_COEFF 1
        avgSubImage[0]->AverageImage(framePtr, rowWidth, 0, 0, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[1]->AverageImage(framePtr, rowWidth, width-subWidth, 0, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[2]->AverageImage(framePtr, rowWidth, 0, height-subHeight, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[3]->AverageImage(framePtr, rowWidth, width-subWidth, height-subHeight, subWidth, subHeight, AVERAGE_COEFF);
#endif

    if ((frame->frameNumber % (commDetectLogoSampleSpacingInFrames)) == 0)
    {
#if !AVG_MODE
//#define AVERAGE_COEFF commDetectLogoSampleAveraging
#define AVERAGE_COEFF 1
        avgSubImage[0]->AverageImage(framePtr, rowWidth, 0, 0, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[1]->AverageImage(framePtr, rowWidth, width-subWidth, 0, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[2]->AverageImage(framePtr, rowWidth, 0, height-subHeight, subWidth, subHeight, AVERAGE_COEFF);
        avgSubImage[3]->AverageImage(framePtr, rowWidth, width-subWidth, height-subHeight, subWidth, subHeight, AVERAGE_COEFF);
#endif

#if 1
        int equalEdges[4];
        int persistenceFactor[4];
        CAVPicture tamask[4];
        CAVPicture calcPic;
        ComponentList newmasklist[4];
#define USE_MASK_AREA 0
#if USE_MASK_AREA
        int32_t maskArea[4];
#endif

        for(i=0; i<4; i++)
        {
#if USE_MASK_AREA
            maskArea[i] = 0;
#endif
            edges = edgeDetector->detectEdges(&avgSubImage[i]->avpic, subHeight, FRAMESGMPCTILE);
            if (edges)
            {
                //equalEdges[i] = memcmp(previousEdges[i]->avpic.data[0], edges->data[0], subWidth*subHeight)==0;
#if 0
                persistenceFactor[i] = previousEdges[i]->IntersectionRatio(edges);
#endif
                previousEdges[i]->copy(edges);

                //previousEdges[i]->Close(5,5);
                //previousEdges[i]->Open(5,5);
#if 0
                tamask[i].copy(*previousEdges[i]);
                tamask[i].Close(5,5);
                tamask[i].Fill();
                tamask[i].Open(5,5);
#else
                timeAveragedEdges[i]->Average(&previousEdges[i]->avpic, commDetectLogoSampleAveraging);
                timeAveragedThresholdedEdges[i]->copy(*timeAveragedEdges[i]);
                timeAveragedThresholdedEdges[i]->Threshold(8);
                tamask[i].copy(*timeAveragedThresholdedEdges[i]);
                tamask[i].Close(5,5);
                tamask[i].Fill();
                tamask[i].Open(5,5);
                // componentize
                tamask[i].GetComponents(newmasklist[i], timeAveragedThresholdedEdges[i]);
#endif
                // check for stability in candidate mask list

#if 1
                calcPic.copy(*previousEdges[i]);
                calcPic.Intersection(tamask[i]);
                persistenceFactor[i] = previousCalcImg[i]->IntersectionRatio(&calcPic.avpic);
                //persistenceFactor[i] = previousCalcImg[i]->SimilarityRatio(&tamask[i].avpic, &calcPic.avpic);
                previousCalcImg[i]->copy(calcPic);
                equalEdges[i] = calcPic.CalcArea(128);
#endif

#if USE_MASK_AREA
                maskArea[i] = tamask[i].CalcArea(128);
#endif
#if 0
                tamask[i].Close(3,3);
                tamask[i].Fill();
                tamask[i].Open(3,3);
#endif
                if (persistenceFactor[i] >= 70)
                {
                    persistenceCount[i]++;
                }
                else
                {
                    persistenceCount[i] = 0;
                }

                //
                // check if any in new mask list matches a registered mask
                //
            }

        }
#if 0
        if (frame->frameNumber == 8*60*25)
        {
            CAVPicture pic1(framePtr, rowWidth, height);
            pic1.writeJPG("f12000");
            for(i=0; i<4;i++)
            {
                avgSubImage[i]->writeJPG(QString("f12000_sub%1").arg(i));
                previousEdges[i]->writeJPG(QString("f12000_edge%1").arg(i));
                previousCalcImg[i]->writeJPG(QString("f12000_pci%1").arg(i));
                timeAveragedEdges[i]->writeJPG(QString("f12000_taedge%1").arg(i));
                timeAveragedThresholdedEdges[i]->writeJPG(QString("f12000_tatedge%1").arg(i));
                tamask[i].writeJPG(QString("f12000_tamask%1").arg(i));
            }
        }
#endif
        LOG(VB_COMMFLAG, LOG_INFO, QString("LogoDet factors %1(%2,%9) %3(%4,%10) %5(%6,%11) %7(%8,%12)")
        //LOG(VB_COMMFLAG, LOG_INFO, QString("LogoDet factors %1(%2) %3(%4) %5(%6) %7(%8)")
                .arg(persistenceFactor[0])
                .arg(persistenceCount[0])
                .arg(persistenceFactor[1])
                .arg(persistenceCount[1])
                .arg(persistenceFactor[2])
                .arg(persistenceCount[2])
                .arg(persistenceFactor[3])
                .arg(persistenceCount[3])
#if 1
                .arg(equalEdges[0])
                .arg(equalEdges[1])
                .arg(equalEdges[2])
                .arg(equalEdges[3])
#endif
#if USE_MASK_AREA
                .arg(maskArea[0])
                .arg(maskArea[1])
                .arg(maskArea[2])
                .arg(maskArea[3])
#endif
                );
#endif
    }
    return false;
}

bool NextgenLogoDetector2::pixelInsideLogo(unsigned int x, unsigned int y)
{
    return false;
}

