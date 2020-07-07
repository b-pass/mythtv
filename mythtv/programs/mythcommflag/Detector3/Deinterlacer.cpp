#include <iostream>

#include "filtermanager.h"
#include "mythmiscutil.h"
#include "mythcontext.h"
#include "mythframe.h"

#include "Deinterlacer.h"

Deinterlacer::Deinterlacer()
{
	m_filterName = gCoreContext->GetSetting("CommDetectDeinterlacer", 
		"kerneldeint");
}

Deinterlacer::~Deinterlacer()
{
}

void Deinterlacer::processFrame(VideoFrame *frame)
{
	if (frame->interlaced_frame)
	{
		if (!m_deint)
		{
			setup(frame);
			if (!m_deint)
				return;
		}
		m_deint->ProcessFrame(frame, kScan_Interlaced);
	}
	else
	{
		if (m_deint)
			m_deint->ProcessFrame(frame, kScan_Progressive);
	}
}

void Deinterlacer::setup(VideoFrame *frame)
{
	if (m_filterName.size() <= 0)
		return;
	
	if (!m_manager)
		m_manager.reset(new FilterManager);
	
	VideoFrameType in_fmt = frame->codec;
	VideoFrameType out_fmt = frame->codec;
	int out_bufsize = 0;
	int width = frame->width;
	int height = frame->height;
	
	m_deint.reset(m_manager->LoadFilters(m_filterName,
		in_fmt, out_fmt,
		width, height, out_bufsize, 1));
    
	if (!m_deint)
	{
		LOG(VB_GENERAL, LOG_ERR, 
			QString("Couldn't load deinterlace filter %1")
				.arg(m_filterName));
		m_filterName.clear();
	}
	else
	{
		LOG(VB_GENERAL, LOG_INFO, 
			QString("Will deinterlace with %1")
				.arg(m_filterName));
	}
}
