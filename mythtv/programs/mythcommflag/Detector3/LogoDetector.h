#ifndef MCF_CD3_LOGO_DETECTOR_H_
#define MCF_CD3_LOGO_DETECTOR_H_

#include <iosfwd>
#include <stdint.h>
#include <QVector>
#include <QPair>

#include "FrameMetadata.h"

class LogoDetector
{
public:
	LogoDetector();
	
	void searchFrame(uint8_t const *buffer, uint32_t width, uint32_t height, uint32_t stride);
	bool detectLogo();
	void processFrame(FrameMetadata &meta, 
						uint8_t const *buffer,
						uint32_t width,
						uint32_t height,
						uint32_t stride) const;
	void dumpLogo(std::ostream &out);
	
private:
	void discriminateEdges(int min_percent);
	
	uint32_t m_frames;
	QVector< QVector<uint32_t> > m_counts;
	QVector< QPair<uint32_t,uint32_t> > m_edges;
	
	bool m_searchBlanks;
};

#endif//MCF_CD3_LOGO_DETECTOR_H_
