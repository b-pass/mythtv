#include "mythcontext.h"
#include "BlankDetector.h"

// NOTE: ClassicDetector bug uses !aggressive instead of aggressive

BlankDetector::BlankDetector()
/* The "Classic" detector uses 'aggressive' in the opposite way most 
 * English speakers would expect.  I think the original author had the 
 * words "aggressive" and "strict" confused. In order to maintain 
 * settings compatibility, we keep with the "Classic" meaning of this 
 * setting here.  But in the code, we use the word "strict" and we use 
 * it in the common understanding of its definition. */
	: m_strict(gCoreContext->GetNumSetting("AggressiveCommDetect", 1) != 0)
	, m_maxAverage(gCoreContext->GetNumSetting("CommDetectDimAverage", 32))
	, m_maxVariance(gCoreContext->GetNumSetting("CommDetectBlankFrameMaxDiff", 10))
{
}

void BlankDetector::processFrame(FrameMetadata &meta,
									uint8_t const *buf,
									uint32_t width,
									uint32_t height,
									uint32_t stride) const
{
	/* 
	Classic method is any of these:
		variance and max is "dim"
		aggressive and variance
		aggresive and max is "dark"
		aggressive and max,average are both "dim"
		
	default dim is 120, dark is 80
	*/
	
	meta.blank = meta.blank 
		|| (meta.brightnessAverage <= m_maxAverage &&
			meta.brightnessVariance <= m_maxVariance)
		|| (!m_strict &&
			meta.brightnessAverage <= m_maxAverage)
		|| (!m_strict &&
			meta.brightnessAverage <= m_maxAverage*2 &&
			meta.brightnessVariance < m_maxVariance/2)
	;
	
	
	if (meta.brightMinX >= (width * 10/100) && meta.brightMinX <= (width * 15/100) &&
		meta.brightMaxX >= (width * 85/100) && meta.brightMaxX <= (width * 90/100))
	{
		// check for lines going down both sides of the frame
		uint32_t total = 0, good = 0;
		for (uint32_t y = height * 15/100; y < height * 85/100; ++y)
		{
			uint8_t d1, d2, b1, b2;
			d1 = buf[y * stride + meta.brightMinX - 2];
			b1 = buf[y * stride + meta.brightMinX + 2];
			b2 = buf[y * stride + meta.brightMaxX - 2];
			d2 = buf[y * stride + meta.brightMaxX + 2];
			
			total += 2;
			if (d1 < 32 && b1 > 32)
				++good;
			if (d2 < 32 && b2 > 32)
				++good;
		}
		
		if (good > (total * 75 / 100))
			meta.format |= FF_PILLARBOX;
	}
	
	if (meta.brightMinY >= (height * 10/100) && meta.brightMinY <= (height * 15/100) &&
		meta.brightMaxY >= (height * 85/100) && meta.brightMaxY <= (height * 90/100))
	{
		// check for lines going across both top/bottom of the frame
		uint32_t total = 0, good = 0;
		for (uint32_t x = width * 15/100; x < width * 85/100; ++x)
		{
			uint8_t d1, d2, b1, b2;
			d1 = buf[(meta.brightMinY - 2) * stride + x];
			b1 = buf[(meta.brightMinY + 2) * stride + x];
			b2 = buf[(meta.brightMaxY - 2) * stride + x];
			d2 = buf[(meta.brightMaxY + 2) * stride + x];
			
			total += 2;
			if (d1 < 32 && b1 > 32)
				++good;
			if (d2 < 32 && b2 > 32)
				++good;
		}
		
		if (good > (total * 75 / 100))
			meta.format |= FF_LETTERBOX;
	}
}
