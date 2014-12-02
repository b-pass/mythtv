#include <cmath>
#include <iostream>

#include "mythcontext.h"
#include "SceneDetector.h"

SceneDetector::SceneDetector()
	: m_maxAge(gCoreContext->GetNumSetting("CommDetectMaxSceneAge", 35))
	, m_maxScenes(gCoreContext->GetNumSetting("CommDetectMaxSceneTracking", 5))
{
	if (m_maxScenes < 2)
		m_maxScenes = 2;
}

void SceneDetector::processFrame(FrameMetadata &meta, 
									uint8_t const *buf,
									uint32_t width,
									uint32_t height,
									uint32_t stride)
{
	if (meta.blank)
	{
		// Blank checking ignores times and extended scene history
		if (!m_scenes.empty() && m_scenes[0].blank)
		{
			// A continuation of an already blank scene
			m_scenes[0] = meta;
		}
		else
		{
			// A blank scene is starting
			meta.scene = true;
			m_scenes.push_front(meta);
		}
		return;
	}
	
	if (m_scenes.size() > 1 && 
		!m_scenes[0].blank &&
		m_scenes[0].frameTime - m_scenes[1].frameTime < 0.3333)
	{
		// Don't change the scene so quickly
		m_scenes[0] = meta;
		return;
	}
	
	for (int i = 0; i < m_scenes.size(); ++i)
	{
		if (meta.frameTime - m_scenes[i].frameTime > m_maxAge)
			break;
		
		if (!different(m_scenes[i], meta))
		{
			// Part of a previous scene, delete the old entry
			m_scenes.removeAt(i);
			m_scenes.push_front(meta);
			return;
		}
	}
	
	/*fprintf(stderr, "%06ld|%5.02lfs|%4.01lf|%10.07lf new scene\n",
					meta.frameNumber + 30,
					meta.frameTime,
					meta.brightnessAverage,
					meta.brightnessVariance);
	
	for (int i = 0; i < m_scenes.size(); ++i)
		fprintf(stderr, "    %d) %06ld|%5.02lfs|%4.01lf|%10.07lf no match\n",
						i,
						m_scenes[i]->frameNumber + 30,
						m_scenes[i]->frameTime,
						m_scenes[i]->brightnessAverage,
						m_scenes[i]->brightnessVariance);*/
	
	// No match, this is a scene change.
	meta.scene = true;
	m_scenes.push_front(meta);
	
	// Keep the scene tracking down to the right size
	while (m_scenes.size() >= m_maxScenes)
		m_scenes.pop_back();
}

bool SceneDetector::different(FrameMetadata const &a, FrameMetadata const &b) const
{
	return abs(a.brightnessAverage - b.brightnessAverage) > 5 ||
				abs(a.brightnessVariance - b.brightnessVariance) > 3;
}
