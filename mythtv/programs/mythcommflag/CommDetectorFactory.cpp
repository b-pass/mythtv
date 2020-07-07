#include "CommDetectorFactory.h"
#include "Classic/ClassicCommDetector.h"
#include "Detector2/CommDetector2.h"
#include "PrePostRoll/PrePostRollFlagger.h"
#include "NextGen/NextgenCommDetector.h"
#include "Detector3/CommDetector3.h"

class MythPlayer;
class RemoteEncoder;

CommDetectorBase*
CommDetectorFactory::makeCommDetector(
    SkipType commDetectMethod,
    bool showProgress, bool fullSpeed,
    MythPlayer* player,
    int chanid,
    const QDateTime& startedAt,
    const QDateTime& stopsAt,
    const QDateTime& recordingStartedAt,
    const QDateTime& recordingStopsAt,
    bool useDB)
{
    if(commDetectMethod & COMM_DETECT_PREPOSTROLL)
    {
        return new PrePostRollFlagger(commDetectMethod, showProgress, fullSpeed,
                                      player, startedAt, stopsAt,
                                      recordingStartedAt, recordingStopsAt);
    }

    if ((commDetectMethod & COMM_DETECT_2))
    {
        return new CommDetector2(
            commDetectMethod, showProgress, fullSpeed,
            player, chanid, startedAt, stopsAt,
            recordingStartedAt, recordingStopsAt, useDB);
    }

    if ((commDetectMethod & COMM_DETECT_NG))
    {
        return new NextgenCommDetector(commDetectMethod, showProgress, fullSpeed,
                player, chanid, startedAt, stopsAt, recordingStartedAt, recordingStopsAt);
    }
    
    if ((commDetectMethod & COMM_DETECT_3))
    {
        return new CommDetector3(commDetectMethod, showProgress, fullSpeed,
                player, chanid, startedAt, stopsAt, recordingStartedAt, recordingStopsAt);
    }

    return new ClassicCommDetector(commDetectMethod, showProgress, fullSpeed,
            player, startedAt, stopsAt, recordingStartedAt, recordingStopsAt);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
