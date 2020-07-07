/*
 * AudioChangeDetector
 *
 * Detect audi changes
 */

#ifndef __AUDIOCHANGEDETECTOR_H__
#define __AUDIOCHANGEDETECTOR_H__

#include "AudioChangeDetectorBase.h"

class AudioChangeDetector : public AudioChangeDetectorBase
{
public:
    AudioChangeDetector();
    virtual ~AudioChangeDetector() {}

    void processFrame(unsigned char *frame);

};

#endif  /* !__AUDIOCHANGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
