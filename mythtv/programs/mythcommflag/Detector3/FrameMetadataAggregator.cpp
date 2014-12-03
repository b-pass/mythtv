#include <math.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "mythcontext.h"
#include "programtypes.h"

#include "FrameMetadataAggregator.h"

FrameMetadataAggregator::FrameMetadataAggregator()
{
	m_currentFormat = FF_NORMAL;
	m_frameRate = 1.0;
	m_logo = false;
	m_scene = false;
	
	m_maxBreakLength = gCoreContext->GetNumSetting("CommDetectMaxCommBreakLength", 330);
	m_minBreakLength = gCoreContext->GetNumSetting("CommDetectMinCommBreakLength", 58);
	m_minShowLength = gCoreContext->GetNumSetting("CommDetectMinShowLength", 58);
	m_maxSingleCommLength = gCoreContext->GetNumSetting("CommDetectMaxCommLength", 0);
	
	m_prev.frameNumber = 0;
	m_prev.blank = true;
}

void FrameMetadataAggregator::add(FrameMetadata const &meta)
{
	if (meta.blank)
	{
		m_prev = meta;
		return;
	}
	
	if (m_prev.blank)
	{
		m_segments.push_back(ShowSegment());
		m_segments.back().frameStart = meta.frameNumber;
	}
	
	ShowSegment &seg = m_segments.back();
	
	seg.frameStop = meta.frameNumber;
	
	if (meta.frameNumber > 1)
	{
		if (meta.aspect != m_prev.aspect)
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

void FrameMetadataAggregator::calculateBreakList(frm_dir_map_t &output) const
{
	QList<ShowSegment> const &segments = coalesce();
	
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
			for (int i = b + 1; i < e; ++i)
			{
				segments[b] += segments[b+1];
				segments.removeAt(b+1);
			}
			segments[b].score = INT_MIN;
		}
	}
	
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
			
			/* If we got here then converting the show to a commercial 
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
	seg.score = 0;
	
	uint64_t frameCount = seg.frameStop - seg.frameStart + 1;
	double segTime = frameCount / m_frameRate;
	if (segTime > m_maxBreakLength)
	{
		seg.score = 1000;
		return;
	}
	
	switch ((int)round(segTime))
	{
		case 15:
		case 29: case 30: case 31:
		case 59: case 60: case 61:
		case 89: case 90: case 91:
		case 119:case 120:case 121:
			// very common lengths 
			seg.score -= 15;
			break;;
		
		case 5:	
		case 10:
		case 20:
		case 40:
		case 45:
			// less common, but still common-ish
			seg.score -= 10;
			break;
		
		default:
			if (segTime < m_minShowLength)
				seg.score -= 5;
			else if (m_maxSingleCommLength && segTime > m_maxSingleCommLength)
				seg.score += 100;
			else if (segTime > 121)
				seg.score += 15;
			break;
	}
	
	if (m_logo)
	{
		if (segTime < 5)
			seg.score += (signed(seg.logoCount * 100 / frameCount) - 50) / 5;
		else
			seg.score += signed(seg.logoCount * 100 / frameCount) - 30;
	}
	
	if (m_scene)
	{
		double secs_per_scene = seg.sceneCount ? segTime / seg.sceneCount : segTime;
		if (secs_per_scene < 2 && segTime >= 1)
			seg.score -= 15;
		else if (secs_per_scene < 3)
			seg.score -= 10;
		else if (secs_per_scene >= 6)
			seg.score += 15;
		else if (secs_per_scene > 5)
			seg.score += 10;
	}
	
	seg.score -= std::min(10u, (seg.formatChanges + seg.sizeChanges)) * 10;
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
			"%13s (%5s): %6s = %5s, %4s, %1s %s\n",
			"Frames",
			"Time",
			"Score",
			"Scene",
			"Logo",
			"FC",
			"Recalc");
		out << buffer;
	}
	
	for (int i = 0; i < segments.size(); ++i)
	{
		ShowSegment seg = segments[i]; // not a ref!
		char const *recalc = "";
		if (seg.score == INT_MIN)
		{
			calculateSegmentScore(seg);
			recalc = "***";
		}
		
		uint64_t totalFrames = seg.frameStop - seg.frameStart + 1;
		double totalTime = totalFrames / m_frameRate;
		double secs_per_scene = seg.sceneCount ? totalTime / seg.sceneCount : -1.0;
		
		snprintf(buffer, sizeof(buffer),
			"%6ld-%6ld (%5.01lfs): %-6d = %5.01lf, %3ld%%, %1d %s\n",
			seg.frameStart, seg.frameStop, totalTime,
			seg.score,
			secs_per_scene,
			seg.logoCount * 100 / (totalFrames ? totalFrames : 1),
			seg.formatChanges + seg.sizeChanges,
			recalc);
		out << buffer;
	}
	out << "\n";
}

void FrameMetadataAggregator::configure(double frameRate, bool logo, bool scene)
{
	m_frameRate = std::max(frameRate, 1.0);
	m_logo = logo;
	m_scene = scene;
}
