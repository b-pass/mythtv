#ifndef MCF_COMMDETECTOR3_H_
#define MCF_COMMDETECTOR3_H_

#include <fstream>
#include <stdint.h>
#include <QObject>
#include <QDateTime>
#include <QScopedPointer>
#include "CommDetectorBase.h"

class MythPlayer;
class BlankDetector;
class SceneDetector;
class LogoDetector;
class AudioDetector;
class FrameMetadataAggregator;

class CommDetector3 : public CommDetectorBase
{
    Q_OBJECT

    public:
        CommDetector3(SkipType commDetectMethod,
						bool showProgress,
						bool fullSpeed,
						MythPlayer* player,
						int chanid,
						const QDateTime& startedAt_in,
						const QDateTime& stopsAt_in,
						const QDateTime& recordingStartedAt_in,
						const QDateTime& recordingStopsAt_in);
		~CommDetector3();
		
        void deleteLater();

        bool go();
        void GetCommercialBreakList(frm_dir_map_t &comms);
        void recordingFinished(long long totalFileSize);
        void requestCommBreakMapUpdate(void);
        void PrintFullMap(
            ostream &out, const frm_dir_map_t *comm_breaks,
            bool verbose) const;
		
	protected:		
		void doUpdate();
		void calculateBreakList(frm_dir_map_t &breakMap);
		void preSearchForLogo();
		bool processAll();

		SkipType m_method;
		bool m_showProgress, m_fullSpeed;
        MythPlayer *m_player;
        QDateTime m_start, m_recordingStart, m_recordingStop;
        bool m_stillRecording, m_wantUpdates, m_needUpdate;
		
		QScopedPointer<BlankDetector> m_blankDet;
		QScopedPointer<SceneDetector> m_sceneDet;
		QScopedPointer<LogoDetector> m_logoDet;
                QScopedPointer<AudioDetector> m_audioDet;
		QScopedPointer<FrameMetadataAggregator> m_aggregator;
		
		std::ofstream m_frameLog;
		frm_dir_map_t m_oldMap;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
