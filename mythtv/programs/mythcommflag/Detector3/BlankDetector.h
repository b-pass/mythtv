#ifndef MCF_CD3_BLANK_DETECTOR_H_
#define MCF_CD3_BLANK_DETECTOR_H_

#include <stdint.h>
#include "FrameMetadata.h"

class BlankDetector
{
public:
	BlankDetector();
	
	void processFrame(FrameMetadata &meta, 
						uint8_t const *buf,
						uint32_t width,
						uint32_t height,
						uint32_t stride) const;

private:
	bool m_strict;
	uint8_t m_maxAverage;
	double m_maxVariance;
};

#endif//MCF_CD3_BLANK_DETECTOR_H_
