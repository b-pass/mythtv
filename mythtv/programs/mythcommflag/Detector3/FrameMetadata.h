#ifndef MCF_CD3_FRAME_METADATA_H_
#define MCF_CD3_FRAME_METADATA_H_

#include <stdint.h>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "mythframe.h"

#ifndef CHANNELS_MAX
#define CHANNELS_MAX 8
#endif

enum FrameFormat
{
	FF_NORMAL    = 0,
	FF_LETTERBOX = 0x1,
	FF_PILLARBOX = 0x2,
	
	FF_MAYBE_LETTERBOX = 0x10,
	FF_MAYBE_PILLARBOX = 0x20,
};

struct FrameMetadata
{
	uint64_t frameNumber;
    int64_t timeCode;
	double frameTime;
	int format;
	float aspect;
	
	double brightnessAverage;
	double brightnessVariance;
	
	uint32_t brightMinX, brightMaxX;
	uint32_t brightMinY, brightMaxY;
	
	bool logo;
	bool scene;
	bool blank;

    int numChannels;
    int16_t peakAudio[CHANNELS_MAX+1];
	
	FrameMetadata()
	{
		memset(this, 0, sizeof(*this));
	}
	
	void Init(VideoFrame *video, uint8_t const *buf, uint32_t width, uint32_t height, uint32_t stride, double fps, double in_aspect)
	{
		aspect = in_aspect;
		frameNumber = video->frameNumber;
        timeCode = video->timecode;
		frameTime = frameNumber / fps;
		format = FF_NORMAL;
		logo = scene = blank = false;
		
		uint32_t brightnessCount[256];
		memset(brightnessCount, 0, sizeof(brightnessCount));
		
		brightMinX = width;
		brightMinY = height;
		brightMaxX = 0;
		brightMaxY = 0;
		uint32_t total = 0;
		uint64_t sum = 0;
		for (uint32_t y = 2; y < height - 2; ++y)
		{
			uint32_t rowstart = y * stride;
			bool bright = false;
			for (uint32_t x = 2; x < width - 2; ++x)
			{
				uint8_t b = buf[rowstart + x];
				brightnessCount[b]++;
				total++;
				sum += b;
				
				if (b >= 32)
				{
					bright = true;
					if (x < brightMinX)
						brightMinX = x;
					else if (x > brightMaxX)
						brightMaxX = x;
				}
			}
			
			if (bright)
			{
				if (y < brightMinY)
					brightMinY = y;
				else if (y > brightMaxY)
					brightMaxY = y;
			}
		}
		
		if (total > 1)
		{
			brightnessAverage = sum / static_cast<double>(total);
			
			for (int i = 0; i < 256; ++i)
			{
				int diff = i - brightnessAverage;
				brightnessVariance += diff * diff * brightnessCount[i];
			}
			brightnessVariance = sqrt(brightnessVariance / (total - 1));
		}
		else
		{
			// this is what static looks like
			brightnessAverage = 127;
			brightnessVariance = 127*127;
		}
	}
	
	static void printHeader(std::ostream &os)
	{
		os 
			<< "Number "
			<< "Time "
			<< "Format "
			<< "Aspect "
			<< "BrightAvg "
			<< "BrightVar "
			<< "BrightLeft "
			<< "BrightRight "
			<< "BrightTop "
			<< "BrightBottom "
			<< "Logo "
			<< "Scene "
			<< "Blank "
            << "#ch "
            << "Peaks "
            << "TimeCode "
			<< "\n"
		;
	}
	
	friend std::ostream &operator << (std::ostream &os, FrameMetadata const &meta)
	{
		os 
			<< meta.frameNumber << " " 
			<< meta.frameTime << " "
			<< meta.format << " "
			<< meta.aspect << " "
			<< meta.brightnessAverage << " "
			<< meta.brightnessVariance << " "
			<< meta.brightMinX << " "
			<< meta.brightMaxX << " "
			<< meta.brightMinY << " "
			<< meta.brightMaxY << " "
			<< (meta.logo ? "Y" : "N") << " "
			<< (meta.scene ? "Y" : "N") << " "
			<< (meta.blank ? "Y" : "N") << " "
            << meta.numChannels
		;
        for (int c = 0; c < meta.numChannels; c++)
            os << " " << meta.peakAudio[c];
        os << " " << meta.timeCode;
        return os;
	}
	
	friend std::istream &operator >> (std::istream &is, FrameMetadata &meta)
	{
		char cl, cs, cb;
		cl = cs = cb = 'N';

		memset(&meta, 0, sizeof(meta));
		is 
			>> meta.frameNumber
			>> meta.frameTime
			>> meta.format
			>> meta.aspect
			>> meta.brightnessAverage
			>> meta.brightnessVariance
			>> meta.brightMinX
			>> meta.brightMaxX
			>> meta.brightMinY
			>> meta.brightMaxY
			>> cl >> cs >> cb
            >> meta.numChannels
		;

        for (int c = 0; c < meta.numChannels; c++)
            is >> meta.peakAudio[c];

        is >> meta.timeCode;
		
		meta.logo = cl != 'N';
		meta.scene = cs != 'N';
		meta.blank = cb != 'N';
		
		return is;
	}
};

#endif//MCF_CD3_FRAME_METADATA_H_
