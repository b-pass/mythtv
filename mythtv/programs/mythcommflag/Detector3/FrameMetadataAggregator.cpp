#include <math.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "mythcontext.h"
#include "programtypes.h"

#include "FrameMetadataAggregator.h"

bool FrameMetadataAggregator::scoreDebugging = false;

FrameMetadataAggregator::FrameMetadataAggregator()
{
	m_currentFormat = FF_NORMAL;
    m_showHasCenterAudio = false;
	m_frameRate = 1.0;
	m_logo = false;
	m_scene = false;
    m_audio = false;
	
	m_maxBreakLength = gCoreContext->GetNumSetting("CommDetectMaxCommBreakLength", 330);
	m_minBreakLength = gCoreContext->GetNumSetting("CommDetectMinCommBreakLength", 58);
	m_minShowLength = gCoreContext->GetNumSetting("CommDetectMinShowLength", 58);
	m_maxSingleCommLength = gCoreContext->GetNumSetting("CommDetectMaxCommLength", 0);
	
	m_prev.frameNumber = 0;
	m_prev.blank = true;
}

void FrameMetadataAggregator::add(FrameMetadata const &meta)
{
	if (meta.blank && m_segments.empty())
    {
        m_prev = meta;
        return;
    }
    
	if (m_prev.blank && !meta.blank)
	{
		m_segments.push_back(ShowSegment());
		m_segments.back().frameStart = meta.frameNumber;
		m_segments.back().timeStampStart = meta.frameTime;
        m_segments.back().timeCodeStart = meta.timeCode;
	}
	
	ShowSegment &seg = m_segments.back();

    seg.numAudioChannels = std::max(seg.numAudioChannels, meta.numChannels);
    seg.audioCount ++;
    for (int c = 0; c < meta.numChannels; c++)
    {
        seg.peakAudio[c] = std::max(seg.peakAudio[c], meta.peakAudio[c]);
        seg.totalAudio[c] += meta.peakAudio[c];
    }

    if (meta.blank)
    {
        m_prev = meta;
        return;
    }
    
	seg.frameStop = meta.frameNumber;
	
	if (meta.frameNumber > 1)
	{
		if (meta.aspect != m_prev.aspect)
			seg.formatChanges++;
		
                if (m_prev.numChannels != meta.numChannels)
                        seg.formatChanges++;
                
		// Only change format when we are SURE it's different
		int prevFormat = m_currentFormat;
		
		if (prevFormat & FF_LETTERBOX)
		{
			if (!(meta.format & (FF_LETTERBOX|FF_MAYBE_LETTERBOX)))
				m_currentFormat &= ~FF_LETTERBOX;
		}
		else
		{
			if (meta.format & FF_LETTERBOX)
				m_currentFormat |= FF_LETTERBOX;
		}
		
		if (prevFormat & FF_PILLARBOX)
		{
			if (!(meta.format & (FF_PILLARBOX|FF_MAYBE_PILLARBOX)))
				m_currentFormat &= ~FF_PILLARBOX;
		}
		else
		{
			if (meta.format & FF_PILLARBOX)
				m_currentFormat |= FF_PILLARBOX;
		}
		
		if (m_currentFormat != prevFormat)
			seg.formatChanges++;
	}
	else
	{
		m_currentFormat = meta.format & (FF_PILLARBOX|FF_LETTERBOX);
	}
	
	if (meta.scene)
		seg.sceneCount++;
	
	if (meta.logo)
		seg.logoCount++;
	
	m_prev = meta;
}

void FrameMetadataAggregator::calculateBreakList(frm_dir_map_t &output, bool nn)
{
	QList<ShowSegment> const &segments = nn ? nnCoalesce() : coalesce();
	
	bool inBreak = false;
	uint64_t prevStop = 0;
	for (int i = 0; i < segments.size(); ++i)
	{
		ShowSegment const &seg = segments[i];
		if (seg.score < 0)
		{
			if (!inBreak)
			{
				inBreak = true;
				uint64_t frame = seg.frameStart;
				if (frame >= m_frameRate)
				{
					frame -= 1;
					// keep up to 1s of blanks
					if (frame - prevStop >= m_frameRate*2)
						frame -= m_frameRate;
				}
				else
				{
					frame = 0;
				}
				
				output[frame] = MARK_COMM_START;
			}
		}
		else
		{
			if (inBreak)
			{
				inBreak = false;
				
				uint64_t frame;
				// keep up to 1s of blanks
				if (seg.frameStart - prevStop >= m_frameRate*2)
					frame = seg.frameStart - m_frameRate;
				else
					frame = prevStop + 1;
				output[frame] = MARK_COMM_END;
			}
		}
		
		prevStop = seg.frameStop;
	}
	
	if (inBreak)
		output[prevStop] = MARK_COMM_END;
}

QList<ShowSegment> FrameMetadataAggregator::coalesce() const
{
	QList<ShowSegment> segments = m_segments;
	if (segments.empty())
		return segments;
	
    if (m_audio)
    {
        int best = 0;
        for (int s = 1; s < segments.size(); ++s)
        {
            //if (segments[s].score > segments[best].score)
            if ((segments[s].frameStop - segments[s].frameStart) > (segments[best].frameStop - segments[best].frameStart))
                best = s;
        }

        m_showHasCenterAudio =
            segments[best].numAudioChannels > 2 &&
            (segments[best].totalAudio[2]/(segments[best].frameStop-segments[best].frameStart+1)) >= 200;
    }
    
	dump(std::cerr, segments, "original");
	
	// join all consecutive like segments together
	int i = 1;
	
	calculateSegmentScore(segments[0]);
	while (i < segments.size())
	{
		ShowSegment &prev = segments[i - 1];
		ShowSegment &cur = segments[i];
		
		calculateSegmentScore(cur);
		
		if ((prev.score >= 0 && cur.score >= 0) ||
			(prev.score < 0 && cur.score < 0))
		{
			if (cur.score < 0 && (cur.frameStop - prev.frameStart + 1) / m_frameRate > m_maxBreakLength)
			{
				// force positive it if would make a negative seg too long
				cur.score = 0;
				++i;
			}
			else
			{
				prev += cur;
				segments.removeAt(i);
			}
		}
		else
		{
			++i;
		}
	}
	
	dump(std::cerr, segments, "easy merge");
	
	// join runs of small segments together
    bool changed = false;
	for (int b = 0; b < segments.size(); ++b)
	{
		int e = b;
		do
		{
			ShowSegment &next = segments[e];
			double nextTime = (next.frameStop - next.frameStart + 1) / m_frameRate;
			double totalTime = (next.frameStop - segments[b].frameStart + 1) / m_frameRate;
			if ((next.score >= 0 && nextTime >= m_minShowLength) ||
				(next.score  < 0 && nextTime >= m_minBreakLength) ||
				totalTime >= m_maxBreakLength)
			{
				break;
			}
			else
			{
				++e;
			}
		}
		while (e < segments.size());
		
		if (b != e)
		{
            changed = true;
			for (int i = b + 1; i < e; ++i)
			{
				segments[b] += segments[b+1];
				segments.removeAt(b+1);
			}
			segments[b].score = INT_MIN;
		}
	}

    if (changed)
        dump(std::cerr, segments, "declutter");
	
	for (int i = 0; i < segments.size(); ++i)
	{
		ShowSegment &check = segments[i];
		if (check.score != INT_MIN)
			continue;
		
		calculateSegmentScore(check);
		if (i > 0)
		{
			ShowSegment &prev = segments[i-1];
			double newTime = (check.frameStop - prev.frameStart) / m_frameRate;
			if ((check.score >= 0 && prev.score >= 0) ||
				(check.score < 0 && prev.score < 0 && newTime < m_maxBreakLength))
			{
				prev += check;
				segments.removeAt(i);
				--i;
				continue;
			}
		}
		
		if (i+1 < segments.size())
		{
			ShowSegment &next = segments[i+1];
			double newTime = (next.frameStop - check.frameStart) / m_frameRate;
			if ((check.score >= 0 && next.score >= 0) ||
				(check.score < 0 && next.score < 0 && newTime < m_maxBreakLength))
			{
				check += next;
				segments.removeAt(i+1);
				continue;
			}
		}
		
		if (i == 0 || i+1 >= segments.size())
			continue; // too small at beg/end is still OK
		
		/* If we get here then this segment is wedged between 2 other 
		  * of opposite types (e.g. a small show segment between 2 
		  * commercial blocks). */
		double checkTime = (check.frameStop - check.frameStart + 1) / m_frameRate;
		
		if (check.score < 0)
		{
			// A commercial between two show blocks
			if (checkTime >= m_minBreakLength)
				continue; // its a valid break, let it go.
			else
				check.score = 0; // its invalid, turn it into show
		}
		else
		{
			// Show between two commercials
			if (checkTime >= m_minShowLength)
				continue; // valid show segment, let it go
			
			uint64_t newStart = segments[i-1].frameStart;
			uint64_t newStop = segments[i+1].frameStop;
			double newTime = (newStop - newStart + 1) / m_frameRate;
			if (newTime < m_maxBreakLength)
				check.score = -1; // convert it to commercial
			
			/* If we got here then converting the seg to a commercial 
			 * is the right call, but doing so would put us over the 
			 * maxBreakLength.  We err on the side of caution and leave 
			 * a show segment which is too short rather than a 
			 * commercial segment which is too long. */
		}
	}
	
	// Find consecutive "breaks" which would be too long together
	int breakStart = -1;
	for (int i = 0; i < segments.size(); ++i)
	{
		if (segments[i].score < 0)
		{
			if (breakStart < 0)
			{
				breakStart = i;
			}
			else if ((segments[i].frameStop - segments[breakStart].frameStart) / m_frameRate > m_maxBreakLength)
			{
				// This segment would put the break over the size limit
				segments[i].score = 0;
				breakStart = -1;
			}
		}
		else
		{
			breakStart = -1;
		}
	}
	
	dump(std::cerr, segments, "final");
	return segments;
}

void FrameMetadataAggregator::calculateSegmentScore(ShowSegment &seg) const
{
    if (scoreDebugging) std::cerr << std::endl;
    
	seg.score = 0;

	uint64_t frameCount = seg.frameStop - seg.frameStart + 1;
	double segTime = frameCount / m_frameRate;
    
    if (m_audio && segTime > 1.0)
    {
        bool hasCenter = seg.numAudioChannels > 2 &&
                          (seg.totalAudio[2]/seg.audioCount) >= 200;

        if (hasCenter != m_showHasCenterAudio)
        {
            if (scoreDebugging) std::cerr << "Wrong number of audio channels: -25" << std::endl;
            seg.score -= 25;
        }
    }
    
    if (m_audio && seg.numAudioChannels > 2)
    {
        uint64_t center = seg.totalAudio[2] / seg.audioCount;
        uint64_t sides = (seg.totalAudio[0] + seg.totalAudio[1]) / 2 / seg.audioCount;

        if (center >= 200)
        {
            if (sides * 4 <= center)
            {
                if (scoreDebugging) std::cerr << "Center is very loud: +20" << std::endl;
                seg.score += 20;
            }
            else if (sides * 3 <= center)
            {
                if (scoreDebugging) std::cerr << "Center is loud: +10" << std::endl;
                seg.score += 10;
            }
            else if (sides * 2 <= center)
            {
                if (scoreDebugging) std::cerr << "Center is a little loud: +5" << std::endl;
                seg.score += 5;
            }
            else if (sides * 1.5 <= center)
            {
                if (scoreDebugging) std::cerr << "Sides are a little loud: -5" << std::endl;
                seg.score -= 5;
            }
            else if (sides < center)
            {
                if (scoreDebugging) std::cerr << "Sides are loud: -10" << std::endl;
                seg.score -= 10;
            }
            else// if (sides >= center)
            {
                if (scoreDebugging) std::cerr << "Sides are very loud: -20" << std::endl;
                seg.score -= 20;
            }
        }
    }
    
	if (segTime > m_maxBreakLength)
	{
        if (scoreDebugging) std::cerr << "Too long to be a break: +1000" << std::endl;
		seg.score = 1000;
	}
	
	switch ((int)round(segTime))
	{
		case 15:
		case 29: case 30: case 31:
		case 59: case 60: case 61:
		case 89: case 90: case 91:
		case 119:case 120:case 121:
            if (scoreDebugging) std::cerr << "Common break length (" << round(segTime) << "s): -15" << std::endl;
			// very common lengths 
			seg.score -= 15;
			break;;
		
		case 5:	
		case 10:
		case 20:
		case 40:
		case 45:
            if (scoreDebugging) std::cerr << "Semi-common break length (" << round(segTime) << "s): -10" << std::endl;
			// less common, but still common-ish
			seg.score -= 10;
			break;
		
		default:
			if (segTime < m_minShowLength)
            {
                if (scoreDebugging) std::cerr << "Shortish (" << round(segTime) << "s): -5" << std::endl;
                seg.score -= 5;
            }
			else if (m_maxSingleCommLength && segTime > m_maxSingleCommLength)
            {
                if (scoreDebugging) std::cerr << "Longish (" << round(segTime) << "s): +100" << std::endl;
				seg.score += 100;
            }
			else if (segTime > 121)
            {
                if (scoreDebugging) std::cerr << "Long (" << round(segTime) << "s): +15" << std::endl;
				seg.score += 15;
            }
			break;
	}
	
    if (seg.frameStart >= m_segments.back().frameStop*95ull/100ull && segTime >= 14.5 && segTime < 45.5 )
    {
        if (scoreDebugging) std::cerr << "Possible credit roll: +20" << std::endl;
        seg.score += 20;
    }
    
	if (m_logo)
	{
        int logoScore;
		if (segTime < 5)
			logoScore = (signed(seg.logoCount * 100 / frameCount) - 50) / 5;
		else
			logoScore = (signed(seg.logoCount * 100 / frameCount) - 40) / 2;
        seg.score += logoScore;
        if (scoreDebugging) std::cerr << "Logo score: " << (logoScore > 0 ? "+" : "") << logoScore << std::endl;
	}
	
	if (m_scene)
	{
		double secs_per_scene = seg.sceneCount ? segTime / seg.sceneCount : segTime;
		if (secs_per_scene < 2 && segTime >= 1)
        {
            if (scoreDebugging) std::cerr << "Very fast scene change (" << secs_per_scene << "): -15" << std::endl;
			seg.score -= 15;
        }
		else if (secs_per_scene < 3)
        {
            if (scoreDebugging) std::cerr << "Fast scene change (" << secs_per_scene << "): -10" << std::endl;
			seg.score -= 10;
        }
		else if (secs_per_scene >= 6)
        {
            if (scoreDebugging) std::cerr << "Very Slow scene change (" << secs_per_scene << "): +15" << std::endl;
			seg.score += 15;
        }
		else if (secs_per_scene > 5)
        {
            if (scoreDebugging) std::cerr << "Slow scene change (" << secs_per_scene << "): +10" << std::endl;
			seg.score += 10;
        }
	}
	
	int formatScore = std::min(10u, (seg.formatChanges + seg.sizeChanges)) * 10;
    if (scoreDebugging) std::cerr << "Format changes: " << -formatScore << std::endl;
    seg.score -= formatScore;
}

void FrameMetadataAggregator::print(std::ostream &out, bool verbose) const
{
	dump(out, m_segments, "dump", verbose);
}

void FrameMetadataAggregator::dump(
		std::ostream &out, 
		QList<ShowSegment> const &segments, 
		char const *desc, 
		bool verbose) const
{
	out << segments.size() << " " << desc << " show segments:\n";
	
	char buffer[256];
	if (verbose)
	{
		snprintf(buffer, sizeof(buffer),
			"%13s (%6s): %6s = %5s, %4s, %3s %1s %s\n",
			"Frames",
			"Time",
			"Score",
			"Scene",
			"Logo",
			"FC",
			"R",
            "Audio");
		out << buffer;
	}
	
	for (int i = 0; i < segments.size(); ++i)
	{
		ShowSegment seg = segments[i]; // not a ref!
		char const *recalc = "";
		if (seg.score == INT_MIN)
		{
			calculateSegmentScore(seg);
			recalc = "*";
		}
		
		uint64_t totalFrames = seg.frameStop - seg.frameStart + 1;
		double totalTime = totalFrames / m_frameRate;
		double secs_per_scene = m_scene && seg.sceneCount ? totalTime / seg.sceneCount : -1.0;
		
		snprintf(buffer, sizeof(buffer),
			"%6ld-%6ld (%6.01lfs): %-6d = %5.01lf, %3ld%%, %3d %1s",
			seg.frameStart, seg.frameStop, totalTime,
			seg.score,
			secs_per_scene,
			m_logo ? seg.logoCount * 100 / (totalFrames ? totalFrames : 1) : 0,
			seg.formatChanges + seg.sizeChanges,
			recalc);
		out << buffer;

        if (seg.audioCount && seg.numAudioChannels)
        {
            out << " " << seg.numAudioChannels << " ";
            for (int c = 0; c < seg.numAudioChannels; c++)
            {
                snprintf(buffer, sizeof(buffer), " %5d", seg.peakAudio[c]);
                out << buffer;
            }
        
            for (int c = 0; c < seg.numAudioChannels; c++)
            {
                snprintf(buffer, sizeof(buffer), " %7.01f", seg.totalAudio[c] / float(seg.audioCount));
                out << buffer;
            }
        }
        else
        {
            out << " 0";
        }
        
        out << "\n";
	}
	out << "\n";
}

void FrameMetadataAggregator::configure(double frameRate, bool logo, bool scene, bool audio)
{
	m_frameRate = std::max(frameRate, 1.0);
	m_logo = logo;
	m_scene = scene;
    m_audio = audio;
}

void FrameMetadataAggregator::nnPrint(std::ostream &out) const
{
    QList<ShowSegment> oldSegs = coalesce();
    
    if (m_audio)
    {
        int best = 0;
        for (int s = 1; s < m_segments.size(); ++s)
        {
            //if (segments[s].score > segments[best].score)
            if ((m_segments[s].frameStop - m_segments[s].frameStart) > (m_segments[best].frameStop - m_segments[best].frameStart))
                best = s;
        }

        const_cast<FrameMetadataAggregator*>(this)->m_showHasCenterAudio =
            m_segments[best].numAudioChannels > 2 &&
            (m_segments[best].totalAudio[2]/(m_segments[best].frameStop-m_segments[best].frameStart+1)) >= 200;
    }
    
    double duration = m_segments.back().timeStampStart + (m_segments.back().frameStop -  m_segments.back().frameStart) / m_frameRate;
    //out << int(duration / 60.0) << ':' << (duration - int(duration/60.0)*60) << "\n";
	
	for (int i = 0; i < m_segments.size(); ++i)
	{
		ShowSegment seg = m_segments[i]; // not a ref!
		calculateSegmentScore(seg);
        for (int j = 0; j < oldSegs.size(); ++j)
        {
            if (seg.frameStart >= oldSegs[j].frameStart && seg.frameStart < oldSegs[j].frameStop)
                seg.score = oldSegs[j].score < 0 ? -1 : +1;
        }

        double curTime = seg.timeStampStart;
		uint64_t totalFrames = seg.frameStop - seg.frameStart + 1;
		double totalTime = totalFrames / m_frameRate;

        char buffer[64];
		snprintf(buffer, sizeof(buffer),
			"%-5d %02d:%04.01lf %6ld-%-6ld ",
            seg.score,
            int(curTime/60.0), (curTime - int(curTime/60.0)*60),
			seg.frameStart, seg.frameStop
        );
		out << buffer;

        // segment time, max 600s
        out << (std::min(totalTime, 600.0) / 600.0) << " ";

        // segment position (beg/end=1, every 30 mins=0.5, other=0)
        if (curTime + totalTime < 600 || duration - curTime < 600)
            out << "1 ";
        else if (int(curTime)%1800 < 120 || int(curTime)%1800 > 1780 || int(curTime+totalTime)%1800 < 120 || int(curTime+totalTime)%1800 > 1780)
            out << "0.5 ";
        else
            out << "0 ";

        // scenes per second
        out << std::min(totalTime > 0 ? seg.sceneCount / totalTime : 0.0, 1.0) << " ";

        // logo percent
        out << (m_logo && totalFrames ? seg.logoCount / double(totalFrames) : 0.0) << " ";

        // format and size changes, max 4
        out << std::min((seg.formatChanges + seg.sizeChanges) / 4.0, 1.0) << " ";
        
        if (seg.numAudioChannels && seg.audioCount)
        {
            uint64_t lr = 0, lrCount = 1;
            uint64_t c = 0;
            uint64_t surr = 0, surrCount = 0;
            
            lr += seg.totalAudio[0];
            if (seg.numAudioChannels > 1)
            {
                lr += seg.totalAudio[1];
                lrCount++;

                if (seg.numAudioChannels > 2)
                {
                    c = seg.totalAudio[2];

                    for (int j = 3; j < seg.numAudioChannels; j++)
                    {
                        surr += seg.totalAudio[j];
                        surrCount++;
                    }
                }
            }

            out << std::min((lr / (seg.audioCount * lrCount)) / 32768.0, 1.0) << " ";
            out << std::min((c / seg.audioCount) / 32768.0, 1.0) << " ";

            if (surrCount)
                out << std::min((surr / (seg.audioCount * surrCount)) / 32768.0, 1.0) << " ";
            else
                out << "0 ";
        }
        else
        {
            out << "0 0 0 ";
        }

        out << "\n";
	}
    
	out << std::flush;
}

void FrameMetadataAggregator::nnTweak(unsigned int frameStart, int nnScore)
{
    int score;
    if (nnScore < 0)
        score = -1000;
    else if (nnScore > 0)
        score = 1000;
    else
        score = 0;

    for (int i = 0; i < m_segments.size(); ++i)
    {
        if (frameStart < m_segments[i].frameStart)
            break;
        if (frameStart == m_segments[i].frameStart)
        {
            m_segments[i].score = score;
            break;
        }
    }
}

QList<ShowSegment> FrameMetadataAggregator::nnCoalesce() const
{
    QList<ShowSegment> segments = m_segments;
	if (segments.empty())
		return segments;
	
    dump(std::cerr, segments, "postTweak");
    
	// join all consecutive like segments together
	int i = 1;
	while (i < segments.size())
	{
		ShowSegment &prev = segments[i - 1];
		ShowSegment &cur = segments[i];
		
		if ((prev.score >= 0 && cur.score >= 0) ||
			(prev.score < 0 && cur.score < 0))
		{
			if (cur.score < 0 && (cur.frameStop - prev.frameStart + 1) / m_frameRate > m_maxBreakLength)
			{
				++i;
                // merge some of the commercial block together up to the min show seg size
                while (i < segments.size() && segments[i].score < 0 && (cur.frameStop - prev.frameStart + 1) / m_frameRate < m_minShowLength)
                {
                    cur += segments[i];
                    segments.removeAt(i);
                }
                // and change it to be a show segment now
                cur.score = 0;
			}
			else
			{
                // segments are same type so merge them
				prev += cur;
				segments.removeAt(i);
			}
		}
		else
		{
            // segments are different types, leave them alone
			++i;
		}
	}
    
	dump(std::cerr, segments, "easy merge");

    // convert small commercials to show blocks and merge
    i = 1;
	while (i+1 < segments.size())
	{
		ShowSegment &prev = segments[i - 1];
		ShowSegment &cur = segments[i];
        ShowSegment &next = segments[i + 1];

        float curLen = (cur.frameStop - cur.frameStart + 1) / m_frameRate;
		
		if (prev.score >= 0 && cur.score < 0 && next.score >= 0 && curLen < m_minBreakLength)
        {
            //std::cerr << "convert at " << i << "/" << cur.frameStart << " prev=" << prev.score << " cur=" << cur.score << " next=" << next.score << std::endl;
            cur.score = 1; // convert to a show segment
            prev += cur;
            prev += next;
            segments.removeAt(i);
            segments.removeAt(i);
        }
        else
        {
            //std::cerr << "NO convert at " << i << "/" << cur.frameStart << " prev=" << prev.score << " cur=" << cur.score << " next=" << next.score << std::endl;
            ++i;
        }
	}

    // convert small show blocks to commercials and merge
    i = 1;
	while (i+1 < segments.size())
	{
		ShowSegment &prev = segments[i - 1];
		ShowSegment &cur = segments[i];
        ShowSegment &next = segments[i + 1];

        float curLen = (cur.frameStop - cur.frameStart + 1) / m_frameRate;
		
        if (prev.score < 0 && cur.score >= 0 && next.score < 0 && curLen < m_minShowLength)
        {
            cur.score = -1; // convert to a break segment
            prev += cur;
            prev += next;
            segments.removeAt(i);
            segments.removeAt(i);
        }
        else
        {
            ++i;
        }
	}
	
	dump(std::cerr, segments, "nnFinal");
	return segments;
}

