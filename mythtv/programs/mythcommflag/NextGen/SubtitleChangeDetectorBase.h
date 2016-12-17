#ifndef _SUBTITLECHANGEDETECTORBASE_H_
#define _SUBTITLECHANGEDETECTORBASE_H_

#include <QObject>

#include "mythframe.h"
class MythPlayer;

class SubtitleChangeDetectorBase : public QObject
{
    Q_OBJECT

  public:
    SubtitleChangeDetectorBase();

    virtual void processFrame(MythPlayer* player, VideoFrame * frame) = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool subtitleState,
                            float debugValue = 0.0);

  protected:
    virtual ~SubtitleChangeDetectorBase() {}

};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
