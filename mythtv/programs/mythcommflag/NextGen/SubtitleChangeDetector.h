/*
 * SubtitleChangeDetector
 *
 * Detect subtitle changes
 */

#ifndef __SUBTITLECHANGEDETECTOR_H__
#define __SUBTITLECHANGEDETECTOR_H__

#include "SubtitleChangeDetectorBase.h"
#include "subtitlereader.h"

class SubtitleChangeDetector : public SubtitleChangeDetectorBase
{
public:
    SubtitleChangeDetector(MythPlayer* player);
    virtual ~SubtitleChangeDetector() {}

    void processFrame(MythPlayer* player, VideoFrame * frame);

protected:
    void Add(int start, int end);

    struct SubtitleTiming 
    {
        int start;
        int end;
        SubtitleTiming(int start, int end);
    };
    MythDeque<SubtitleTiming> subtitleTimingList;

    bool m_teletextIsBlank;
    bool m_cc608IsBlank;
    bool m_cc708IsBlank;
};

#endif  /* !__SUBTITLECHANGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
