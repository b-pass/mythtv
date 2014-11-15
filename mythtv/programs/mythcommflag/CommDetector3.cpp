#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <QCoreApplication>
#include <QString>
#include <QList>
#include <QtAlgorithms>

#include "mythmiscutil.h"
#include "mythcontext.h"
#include "mythplayer.h"
#include "mythcommflagplayer.h"

#include "FrameMetadata.h"
#include "FrameMetadataAggregator.h"
#include "Deinterlacer.h"
#include "BlankDetector.h"
#include "SceneDetector.h"
#include "LogoDetector.h"
#include "CommDetector3.h"

CommDetector3::CommDetector3(SkipType commDetectMethod_in,
								bool showProgress_in,
								bool fullSpeed_in,
								MythPlayer* player_in,
								int chanid,
								const QDateTime& startedAt_in,
								const QDateTime& stopsAt_in,
								const QDateTime& recordingStartedAt_in,
								const QDateTime& recordingStopsAt_in) 
	: m_method(commDetectMethod_in)
	, m_showProgress(showProgress_in)
	, m_player(player_in)
	, m_start(startedAt_in)
	, m_recordingStart(recordingStartedAt_in)
	, m_recordingStop(recordingStopsAt_in)
	, m_stillRecording(m_recordingStop > MythDate::current())
	, m_wantUpdates(false)
	, m_needUpdate(false)
{
}

CommDetector3::~CommDetector3()
{
}

bool CommDetector3::go()
{
	m_aggregator.reset(new FrameMetadataAggregator());
	m_deinterlacer.reset(new Deinterlacer());
	m_blankDet.reset();
	m_sceneDet.reset();
	m_logoDet.reset();
	
	if (m_method & COMM_DETECT_BLANKS)
		m_blankDet.reset(new BlankDetector());
	
	if (m_method & COMM_DETECT_SCENE)
		m_sceneDet.reset(new SceneDetector());
	
	if (m_method & COMM_DETECT_LOGO)
		m_logoDet.reset(new LogoDetector());

    emit statusUpdate("Building Head Start Buffer");

	// Let the recording lead detection by ~15s    
    int headStartSecs = 15;
    if (m_logoDet)
		headStartSecs += 600; // need 10 minutes for a logo search

    while (m_stillRecording)
    {
		emit breathe();
		if (m_bStop)
			return false;
		sleep(1);
		if (m_recordingStart.secsTo(MythDate::current()) >= headStartSecs)
			break;
	}

    if (m_player->OpenFile() < 0)
        return false;
	
    if (!m_player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }
    
    m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(150));
    m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0)); 
    
    m_player->EnableSubtitles(false);
	m_player->ResetTotalDuration();
	
    if (m_logoDet)
		preSearchForLogo();
	
	if (!processAll())
		return false;
	
	// push a final update
	if (m_wantUpdates)
		doUpdate();
	
    return true;
}

void CommDetector3::preSearchForLogo()
{
	QTime searchTime;
    searchTime.start();
    
	if (!m_logoDet)
		return;
		
	emit statusUpdate(QCoreApplication::translate("(mythcommflag)", "Searching for Logo"));
	std::cerr << "Searching for logo..." << std::endl;
	
	double fps = m_player->GetDecoder()->GetFPS();

	uint64_t start = uint64_t(m_recordingStart.secsTo(m_start) * fps);
	uint64_t postRoll = uint64_t(m_start.secsTo(m_recordingStop) * fps);
	uint64_t stop = start + uint64_t(600 * fps);
	
	if (stop > postRoll)
		stop = postRoll;
	
	int interval = (int)fps;
	if (interval < 2)
		interval = 2;
	
	/* Passing a frame number to GetRawVideoFrame causes frequent 
	asserts in libavformat.  The solution seems to be to read the file 
	linearly and only examine the frames we care about.  This takes 
	twice as long because we have to decompress frames we don't care 
	about, but it doesn't crash. */
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(start)); // seek to start frame?
	for (uint32_t fn = start, count = 0; fn < stop; ++fn, ++count)
	{
		if (m_player->GetEof() != kEofStateNone)
			break;
		
		if (m_bStop)
		{
			m_logoDet.reset();
			return;
		}
		
		VideoFrame* frame = m_player->GetRawVideoFrame();
		
        // Individal frames have a size which can be larger than the actual size of the video
        QSize const &videoSize = m_player->GetVideoSize();
        
		if (frame)
			m_deinterlacer->processFrame(frame);
		
		if (frame && frame->buf && (frame->frameNumber % interval) == 0)
		{
			m_logoDet->searchFrame(frame->buf + frame->offsets[0], 
									videoSize.width(),
									videoSize.height(), 
									frame->pitches[0]);
		}
		
		m_player->DiscardVideoFrame(frame);
		
		if ((count%300) == 0)
		{
			emit breathe();
			if (!m_fullSpeed)
				usleep(10000);
			
			double elapsed = searchTime.elapsed() / 1000.0;
			double fps = elapsed ? count / elapsed : 0;
			std::cerr << "\rLogo Searching: " << (count * 100 / (stop - start)) 
					<< "% at " << fps 
					<< " frames/sec          " << std::flush;
		}
	}
	
	std::cerr << "Search for logo is complete" << std::endl;
	
	if (!m_logoDet->detectLogo())
	{
		emit statusUpdate("Logo not found");
		m_logoDet.reset();
	}
	else
	{
		m_logoDet->dumpLogo();
	}
	
	// reset player to beginning
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0));
	m_player->ResetTotalDuration();
}

bool CommDetector3::processAll()
{
    QTime flagTime;
    flagTime.start();
    
	if (!m_logoDet && !m_sceneDet && !m_blankDet)
		return true;
	
	double fps = m_player->GetDecoder()->GetFPS();
	
    uint64_t totalFrames;
    if (m_recordingStop < MythDate::current())
        totalFrames = m_player->GetTotalFrameCount();
    else
        totalFrames = (uint64_t)(fps * (m_recordingStart.secsTo(m_recordingStop)));
	if (totalFrames == 0)
		totalFrames = 1;
    
    emit statusUpdate("Processing");
    std::cerr << "Starting processing of " << totalFrames << " frames" << std::endl;
	
	m_aggregator->configure(fps, !!m_logoDet, !!m_sceneDet);
	
    uint64_t count = 0;
    while (m_player->GetEof() == kEofStateNone)
    {
		if (m_bStop)
			return false;
		
        while (m_bPaused)
        {
            emit breathe();
            usleep(100000);
            if (m_bStop)
				return false;
        }
        
        // Individal frames have a size which can be larger than the actual size of the video
        QSize const &videoSize = m_player->GetVideoSize();
		VideoFrame *frame = m_player->GetRawVideoFrame();
		uint8_t const *buf;
		uint32_t stride;
		bool wasBlank = true;
		QDateTime frameTime;
		
		/* Sometimes something bad happens in the decoder and it nulls 
		 * out frame->buf in the middle of our using it.  We copy it 
		 * out and use our copy.  That stops the crashes, but who knows 
		 * if the data is still valid. */
		 if (frame)
		 {
			m_deinterlacer->processFrame(frame);
			
			stride = static_cast<uint32_t>(frame->pitches[0]);
			buf = frame->buf;
		}
		else
		{
			buf = NULL;
			stride = 0;
		}
		
		if (buf && stride)
		{
			buf += frame->offsets[0];
			
			FrameMetadata meta;
			meta.Init(frame, buf, videoSize.width(), videoSize.height(), stride, fps, m_player->GetVideoAspect());
			
			frameTime = m_start.addMSecs((uint64_t)(meta.frameTime * 1000));
			if (frameTime >= m_recordingStart)
			{
				if (m_blankDet)
					m_blankDet->processFrame(meta, buf, videoSize.width(), videoSize.height(), stride);
				
				if (m_sceneDet)
					m_sceneDet->processFrame(meta, buf, videoSize.width(), videoSize.height(), stride);
				
				if (m_logoDet)
					m_logoDet->processFrame(meta, buf, videoSize.width(), videoSize.height(), stride);
				
				wasBlank = meta.blank;
				m_aggregator->add(meta);
			}
		}
		else
		{
			FrameMetadata frameDropped;
			frameDropped.blank = wasBlank = true;
			if (frame)
				frameDropped.frameNumber = frame->frameNumber;
			m_aggregator->add(frameDropped);
		}
		m_player->DiscardVideoFrame(frame);
		frame = NULL;
		
		if (frameTime >= m_recordingStop)
			break;
		
		while (m_stillRecording)
		{
			if (frameTime.secsTo(MythDate::current()) >= 10)
				break;
			
			emit breathe();
			usleep(10000);
			if (m_bStop)
				return false;
		}
		
		++count;
		if ((count%300) == 0)
		{
			emit breathe();
			if (!m_stillRecording && !m_fullSpeed)
				usleep(10000);
			if (m_wantUpdates)
				m_needUpdate = true;
			
            int percent = count * 100 / totalFrames;
            float elapsed = flagTime.elapsed() / 1000.0;
            float fps = elapsed != 0 ? count / elapsed : 0;
            
			emit statusUpdate(
				QCoreApplication::translate("(mythcommflag)", "%1% Completed @ %2 fps.")
				.arg(percent).arg(fps));
			
			std::cerr << "\rProgress: " << percent << "%, " << count 
					<< " frames at " << fps 
					<< " frames/sec          " << std::flush;
		}
		
		if (m_needUpdate && wasBlank)
		{
			doUpdate();
			m_needUpdate = false;
		}
		
		/*if (count > 30000)
		{
			std::cerr << "DEBUGGING break early" << std::endl;
			break;
		}*/
	}
	
    emit statusUpdate("Done searching");
    std::cerr << "\nDone\n" << std::endl;
    
	return true;
}

void CommDetector3::doUpdate()
{
	frm_dir_map_t newMap;
	
	m_aggregator->calculateBreakList(newMap);
	
	if (m_oldMap != newMap)
	{
		m_oldMap.swap(newMap);
		emit gotNewCommercialBreakList();
	}
}

void CommDetector3::deleteLater(void)
{
	m_aggregator.reset();
	m_blankDet.reset();
	m_sceneDet.reset();
	m_logoDet.reset();
	
    CommDetectorBase::deleteLater();
}

void CommDetector3::GetCommercialBreakList(frm_dir_map_t &marks)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetCommBreakMap()");

	if (m_aggregator)
		m_aggregator->calculateBreakList(marks);
	else
		marks.clear();
}

void CommDetector3::recordingFinished(long long totalFileSize)
{
    (void)totalFileSize;
    
    m_stillRecording = false;
}

void CommDetector3::requestCommBreakMapUpdate(void)
{
	m_wantUpdates = true;
	m_needUpdate = true;
}

void CommDetector3::PrintFullMap(std::ostream &out,
									frm_dir_map_t const *comm_breaks,
									bool verbose) const
{
    /*if (verbose)
    {
        QByteArray tmp = FrameInfoEntry::GetHeader().toLatin1();
        out << tmp.constData() << " mark" << endl;
    }

    for (long long i = 1; i < curFrameNumber; i++)
    {
        QMap<long long, FrameInfoEntry>::const_iterator it = frameInfo.find(i);
        if (it == frameInfo.end())
            continue;

        QByteArray atmp = (*it).toString(i, verbose).toLatin1();
        out << atmp.constData() << " ";
        if (comm_breaks)
        {
            frm_dir_map_t::const_iterator mit = comm_breaks->find(i);
            if (mit != comm_breaks->end())
            {
                QString tmp = (verbose) ?
                    toString((MarkTypes)*mit) : QString::number(*mit);
                atmp = tmp.toLatin1();

                out << atmp.constData();
            }
        }
        out << "\n";
    }*/

    out << std::flush;
}
