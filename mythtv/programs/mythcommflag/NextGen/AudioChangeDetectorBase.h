#ifndef _AUDIOCHANGEDETECTORBASE_H_
#define _AUDIOCHANGEDETECTORBASE_H_

#include <QObject>

class AudioChangeDetectorBase : public QObject
{
    Q_OBJECT

  public:
    AudioChangeDetectorBase();

    virtual void processFrame(unsigned char *frame) = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool audiochange,
                            float amplitude,
                            float debugValue = 0.0);

  protected:
    virtual ~AudioChangeDetectorBase() {}

};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
