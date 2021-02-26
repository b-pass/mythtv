#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <unistd.h>

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
#include "AudioDetector.h"
#include "BlankDetector.h"
#include "SceneDetector.h"
#include "LogoDetector.h"
#include "CommDetector3.h"

#define FRAME_LOGGING

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
#ifdef FRAME_LOGGING
	QString logFile;
	if (chanid > 0)
		logFile = QString("/tmp/mcf_%1_%2.log").arg(chanid).arg(m_recordingStart.toString("yyyyMMddhhmmss"));
	else
		logFile = "/tmp/mcf_frames.log";
	m_frameLog.open(logFile.toLatin1().data());
	m_frameLog << "# CD3 frame log generated " << MythDate::current().toString().toLatin1().data() << std::endl;
#endif
}

CommDetector3::~CommDetector3()
{
}

bool CommDetector3::go()
{
	m_aggregator.reset(new FrameMetadataAggregator());
	m_blankDet.reset();
	m_sceneDet.reset();
	m_logoDet.reset();
        m_audioDet.reset();
    
	if (m_method & COMM_DETECT_BLANKS)
		m_blankDet.reset(new BlankDetector());
	
	if (m_method & COMM_DETECT_SCENE)
		m_sceneDet.reset(new SceneDetector());
	
	if (m_method & COMM_DETECT_LOGO)
		m_logoDet.reset(new LogoDetector());

    if (m_method & COMM_DETECT_AUDIO)
    {
        m_audioDet.reset(new AudioDetector());
        m_player->GetAudio()->SetAudioOutput(&*m_audioDet);
        m_player->GetAudio()->ReinitAudio();
    }

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
    
    m_player->EnableSubtitles(false);
    m_player->ForceDeinterlacer(false, DEINT_CPU | DEINT_MEDIUM);
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(30));
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
	
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0));
	m_player->ResetTotalDuration();
	
	double fps = m_player->GetFrameRate();

	uint64_t start, stop;
	uint64_t recLen = m_recordingStart.secsTo(m_recordingStop);
	if (m_stillRecording || recLen <= 900)
	{
		start = uint64_t(m_recordingStart.secsTo(m_start) * fps);
		stop = start + uint64_t(600 * fps);
		uint64_t absEnd = uint64_t(recLen * fps);
		if (stop > absEnd)
			stop = absEnd;
	}	
	else
	{
		// Use the end of the first 1/3rd of the show
		start = m_player->GetTotalFrameCount() / 3 - uint64_t(300*fps);
		stop = start + uint64_t(600 * fps);
	}
	
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
	
	bool found = m_logoDet->detectLogo();
	
	m_logoDet->dumpLogo(m_frameLog);
	
	if (!found)
		m_logoDet.reset();
}

static void pgm_save(uint8_t const *buf, int wrap, int xsize, int ysize, char const *filename)
{
    FILE *f;
    int i;

    f = fopen(filename,"w");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

bool CommDetector3::processAll()
{
    QTime flagTime;
    flagTime.start();
    
	if (!m_logoDet && !m_sceneDet && !m_blankDet)
		return true;
    
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(30));
	m_player->ResetTotalDuration();
    
	double fps = m_player->GetFrameRate();

    if (m_audioDet)
        m_audioDet->Enable(fps);
    
	m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0));

    uint64_t totalFrames;
    if (m_recordingStop < MythDate::current())
        totalFrames = m_player->GetTotalFrameCount();
    else
        totalFrames = (uint64_t)(fps * (m_recordingStart.secsTo(m_recordingStop)));
	if (totalFrames == 0)
		totalFrames = 1;
    
    emit statusUpdate("Processing");
    std::cerr << "Starting processing of " << totalFrames << " frames" << std::endl;
    
    emit breathe();
    usleep(100000);
    
    m_frameLog << "FPS=" << fps << std::endl;
    FrameMetadata::printHeader(m_frameLog);
    
	m_aggregator->configure(fps, !!m_logoDet, !!m_sceneDet, !!m_audioDet);
	
    uint64_t count = 0, logoCount = 0;
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
		
                        /*if (frame->frameNumber%30 == 0)
                        {
                                char temp[256];
                                sprintf(temp, "/tmp/debug/%d.pgm", frame->frameNumber);
                                pgm_save(buf, stride, videoSize.width(), videoSize.height(), temp);
                        }*/
                                        
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

                if (m_audioDet)
                    m_audioDet->processFrame(meta);
				
				if (meta.logo)
					++logoCount;
				
				m_frameLog << meta << std::endl;
				
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
			
			m_frameLog << frameDropped << std::endl;
			
			m_aggregator->add(frameDropped);
		}
		m_player->DiscardVideoFrame(frame);
		frame = NULL;
		
		if (m_stillRecording && frameTime >= m_recordingStop)
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
	
	m_frameLog << "# Total logo = " << (logoCount * 100.0 / count) << " percent" << std::endl;
	
	// Don't use logo results if if wasn't very common at all
	if (logoCount < count / 3)
		m_logoDet.reset();
	
	m_aggregator->configure(fps, !!m_logoDet, !!m_sceneDet, !!m_audioDet);
	
    emit statusUpdate("Done searching");
    std::cerr << "\nDone\n" << std::endl;
    
    m_frameLog << std::flush;
	return true;
}

void CommDetector3::doUpdate()
{
	frm_dir_map_t newMap;
	
	m_aggregator->calculateBreakList(newMap, !!(m_method&COMM_DETECT_3_NN));
	
	if (m_oldMap != newMap)
	{
		m_oldMap.swap(newMap);
		emit gotNewCommercialBreakList();
	}
}

void CommDetector3::deleteLater(void)
{
    if (m_player && m_player->GetAudio())
        m_player->GetAudio()->SetAudioOutput(NULL);
    
	m_aggregator.reset();
	m_blankDet.reset();
	m_sceneDet.reset();
	m_logoDet.reset();
    m_audioDet.reset();
	
    CommDetectorBase::deleteLater();
}

void CommDetector3::GetCommercialBreakList(frm_dir_map_t &marks)
{
    LOG(VB_COMMFLAG, LOG_INFO, "CommDetect::GetCommBreakMap()");

    if (!m_aggregator)
    {
        marks.clear();
    }
    else if (m_method & COMM_DETECT_3_NN)
    {
        QString tmp = QString("/tmp/mcf-nn-%1").arg(getpid());
        {
            std::ofstream ofs(qPrintable(tmp));
            m_aggregator->nnPrint(ofs);
        }

        QString cmd = QString("mythcommflagneural %1").arg(tmp);
        FILE *resf = popen(qPrintable(cmd), "r");
        while (!feof(resf))
        {
            unsigned int fs = 0;
            int sc = 0;
            if (fscanf(resf, " %u , %d \n", &fs, &sc) == 2)
                m_aggregator->nnTweak(fs, sc);
            else
                fgetc(resf);
        }
        pclose(resf);
        unlink(qPrintable(tmp));
        
		m_aggregator->calculateBreakList(marks, true);
    }
    else
    {
        m_aggregator->calculateBreakList(marks, false);
    }
    
	m_frameLog << std::endl << marks.size() << " marks:\n";
	frm_dir_map_t::const_iterator it;
	for (it = marks.begin(); it != marks.end(); ++it)
		m_frameLog << "framenum: " << it.key() << "\tmarktype: " << *it << std::endl;
	m_frameLog << std::endl;
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
	if (m_aggregator)
		m_aggregator->print(out, verbose);
	else
		out << "\n(null)\n\n";
    out << std::flush;
}
