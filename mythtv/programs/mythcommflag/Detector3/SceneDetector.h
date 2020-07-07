#ifndef MCF_CD3_SCENE_DETECTOR_H_
#define MCF_CD3_SCENE_DETECTOR_H_

#include <stdint.h>
#include <QList>

#include "FrameMetadata.h"

class SceneDetector
{
public:
	SceneDetector();
	
	void processFrame(FrameMetadata &meta, 
						uint8_t const *buf,
						uint32_t width,
						uint32_t height,
						uint32_t stride);
	
private:
	bool different(FrameMetadata const &a, FrameMetadata const &b) const;

	int m_maxAge;
	int m_maxScenes;
	QList<FrameMetadata> m_scenes;
};

#endif//MCF_CD3_SCENE_DETECTOR_H_
