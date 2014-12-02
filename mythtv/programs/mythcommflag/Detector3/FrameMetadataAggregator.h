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
	uint32_t sizeChanges;
	uint32_t formatChanges;
	uint32_t sceneCount;
	uint32_t logoCount;
	int32_t score;
	
	ShowSegment()
		: frameStart(0)
		, frameStop(0)
		, sizeChanges(0)
		, formatChanges(0)
		, sceneCount(0)
		, logoCount(0)
		, score(INT_MIN)
	{
	}
	
	ShowSegment &operator += (ShowSegment const &seg)
	{
		frameStart = std::min(frameStart, seg.frameStart);
		frameStop = std::max(frameStop, seg.frameStop);
		sizeChanges += seg.sizeChanges;
		formatChanges += seg.formatChanges;
		sceneCount += seg.sceneCount;
		logoCount += seg.logoCount;
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
	FrameMetadataAggregator();
	
	void configure(double frameRate,
					bool logo,
					bool scene);
	void add(FrameMetadata const &meta);
	void calculateBreakList(frm_dir_map_t &breakList) const;
	
	void print(std::ostream &out, bool verbose = false) const;
	
private:
	QList<ShowSegment> coalesce() const;
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
	double m_frameRate;
	bool m_logo;
	bool m_scene;
	
	QList<ShowSegment> m_segments;
};

#endif//MCF_CD3_FRAME_METADATA_AGGREGATOR_H_
