#include <algorithm>
#include <iostream>

#include "mythcorecontext.h"
#include "Sobel.hpp"
#include "LogoDetector.h"

LogoDetector::LogoDetector()
	: m_frames(0)
	, m_searchBlanks(!!gCoreContext->GetNumSetting("CommDetectBlankCanHaveLogo", 0))
{
}

void LogoDetector::searchFrame(uint8_t const *buf, 
								uint32_t width, 
								uint32_t height, 
								uint32_t stride)
{
	/* Borders: because of overscan, nothing is ever in the outside 3.5%
	See also: http://en.wikipedia.org/wiki/Overscan */
	uint32_t ymin = std::max(3u, height * 3/100);
	uint32_t ymax = height - ymin;
	uint32_t xmin = std::max(3u, width * 3/100);
	uint32_t xmax = width - xmin;
	
	//std::cerr << "Searching from " << ymin << " to " << ymax << " of " << height << std::endl;
	
	// The center 1/3 of the screen never contains a logo
	uint32_t cymin = height * 1/3;
	uint32_t cymax = height - cymin;
	uint32_t cxmin = width * 1/3;
	uint32_t cxmax = width - cxmin;
	
	++m_frames;
	m_counts.resize(height);
	
	for (uint32_t y = ymin; y < ymax; ++y)
	{
		m_counts[y].resize(width);
		
		uint32_t x = xmin;
		for ( ; x < cxmin; ++x)
		{
			if (IsSobelEdgeAt(buf, x, y, stride))
				++m_counts[y][x];
		}
		
		if (y >= cymin && y <= cymax)
			x = cxmax; // skip over the middle of the line
		
		for ( ; x < xmax; ++x)
		{
			if (IsSobelEdgeAt(buf, x, y, stride))
				++m_counts[y][x];
		}
	}
}

bool LogoDetector::detectLogo()
{
	uint32_t threshold = m_frames * 60 / 100;
	m_edges.clear();
	
	uint32_t minx = INT_MAX, maxx = 0, miny = INT_MAX, maxy = 0;
	for (int y = 0; y < m_counts.size(); ++y)
	{
		for (int x = 0; x < m_counts[y].size(); ++x)
		{
			if (m_counts[y][x] >= threshold &&
				// dont include edges who are not near anything else
				// +/- is safe because of search borders
				(
					m_counts[y-1][x-1] >= threshold ||
					m_counts[y-1][x+0] >= threshold ||
					m_counts[y-1][x+1] >= threshold ||
					m_counts[y][x-1] >= threshold ||
					m_counts[y][x+1] >= threshold ||
					m_counts[y+1][x-1] >= threshold ||
					m_counts[y+1][x+0] >= threshold ||
					m_counts[y+1][x+1] >= threshold
				))
			{
				minx = std::min(minx, static_cast<uint32_t>(x));
				maxx = std::max(maxx, static_cast<uint32_t>(x));
				miny = std::min(miny, static_cast<uint32_t>(y));
				maxy = std::max(maxy, static_cast<uint32_t>(y));
				
				m_edges.push_back(qMakePair(static_cast<uint32_t>(x),
											static_cast<uint32_t>(y)));
			}
		}
	}
	
	m_counts.clear();
	
	return 
		m_edges.size() >= 20 &&
		(maxx - minx) >= 4 && 
		(maxy - miny) >= 4
	;
}

void LogoDetector::processFrame(FrameMetadata &meta,
									uint8_t const *buf,
									uint32_t width,
									uint32_t height,
									uint32_t stride) const
{
	if (!m_searchBlanks && meta.blank)
		return;
	
	uint32_t need = m_edges.size() * 75/100;
	uint32_t good = 0;
	for (int i = 0; i < m_edges.size(); ++i)
	{
		if (m_edges[i].first < width && 
			m_edges[i].second < height &&
			IsSobelEdgeAt(buf, m_edges[i].first, m_edges[i].second, stride))
		{
			++good;
			if (good >= need)
			{
				meta.logo = true;
				return;
			}
		}
	}
}

void LogoDetector::dumpLogo()
{
	if (m_edges.empty())
	{
		std::cerr << "No logo found" << std::endl;
		return;
	}
	
	uint32_t xmin = INT_MAX;
	for (int i = 0; i < m_edges.size(); ++i)
		xmin = std::min(xmin, m_edges[i].first);
	std::cerr << "Logo (" << m_edges.size() << " edges) at ("
				<< xmin << "," << m_edges[0].second << "):" << std::endl;
	
	uint32_t px = xmin, py = m_edges[0].second;
	for (int i = 0; i < m_edges.size(); ++i)
	{
		while (py < m_edges[i].second)
		{
			px = xmin;
			++py;
			std::cerr << std::endl;
		}
		
		while (px < m_edges[i].first)
		{
			std::cerr << ' ';
			++px;
		}
		
		std::cerr << '+';
		++px;
	}
	std::cerr << "\n\n" << std::endl;
}
