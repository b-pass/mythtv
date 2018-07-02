#ifndef MCF_CD3_FRAME_METADATA_AGGREGATOR_H_
#define MCF_CD3_FRAME_METADATA_AGGREGATOR_H_

#include <algorithm>
#include <iosfwd>
#include <stdint.h>
#include <QList>
#include "FrameMetadata.h"

#include "programtypes.h" // frm_dir_map_t

struct ShowSegment
{
	uint64_t frameStart, frameStop;
    double timeStampStart;
    int64_t timeCodeStart;
	uint32_t sizeChanges;
	uint32_t formatChanges;
	uint32_t sceneCount;
	uint32_t logoCount;
    
    int numAudioChannels;
    int16_t peakAudio[CHANNELS_MAX+1];
    uint64_t totalAudio[CHANNELS_MAX+1];
    uint32_t audioCount;
	
	int32_t score;
    
	ShowSegment()
		: frameStart(0)
		, frameStop(0)
        , timeStampStart(0)
        , timeCodeStart(0)
		, sizeChanges(0)
		, formatChanges(0)
		, sceneCount(0)
		, logoCount(0)
        , numAudioChannels(0)
        , audioCount(0)
		, score(INT_MIN)
	{
        memset(peakAudio, 0, sizeof(peakAudio));
        memset(totalAudio, 0, sizeof(totalAudio));
	}
	
	ShowSegment &operator += (ShowSegment const &seg)
	{
		frameStart = std::min(frameStart, seg.frameStart);
		frameStop = std::max(frameStop, seg.frameStop);
        timeStampStart = std::min(timeStampStart, seg.timeStampStart);
        timeCodeStart = std::min(timeCodeStart, seg.timeCodeStart);
		sizeChanges += seg.sizeChanges;
		formatChanges += seg.formatChanges;
		sceneCount += seg.sceneCount;
		logoCount += seg.logoCount;

        numAudioChannels = std::max(numAudioChannels, seg.numAudioChannels);
        for (int c = 0; c < seg.numAudioChannels; c++)
        {
            peakAudio[c] = std::max(peakAudio[c], seg.peakAudio[c]);
            totalAudio[c] += seg.totalAudio[c];
        }
        audioCount += seg.audioCount;
        
		if (score != INT_MIN && seg.score != INT_MIN)
			score += seg.score;
		else
			score = INT_MIN;
        
		return *this;
	}
};

class FrameMetadataAggregator
{
public:
	static bool scoreDebugging;
	
	FrameMetadataAggregator();
	
	void configure(double frameRate,
			bool logo,
			bool scene,
			bool audio);
	void add(FrameMetadata const &meta);
    
	void calculateBreakList(frm_dir_map_t &breakList, bool nn = false);
	
	void print(std::ostream &out, bool verbose = false) const;

    void nnPrint(std::ostream &out) const;
    void nnTweak(unsigned int frameStart, signed int score);

private:
	QList<ShowSegment> coalesce() const;
	QList<ShowSegment> nnCoalesce() const;
	void calculateSegmentScore(ShowSegment &seg) const;
	void dump(std::ostream &out, 
				QList<ShowSegment> const &segments, 
				char const *desc = "?",
				bool verbose = false) const;
	
	uint32_t m_maxBreakLength;
	uint32_t m_minBreakLength;
	uint32_t m_minShowLength;
	uint32_t m_maxSingleCommLength;
	
	FrameMetadata m_prev;
	int m_currentFormat;
	mutable bool m_showHasCenterAudio;
	double m_frameRate;
	bool m_logo, m_scene, m_audio;
	
	QList<ShowSegment> m_segments;
};

#endif//MCF_CD3_FRAME_METADATA_AGGREGATOR_H_
