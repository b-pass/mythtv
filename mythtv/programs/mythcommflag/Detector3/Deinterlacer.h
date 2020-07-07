#ifndef MCF_DEINTERLACE_H
#define MCF_DEINTERLACE_H

#include <QScopedPointer>
#include <QString>

#include "mythframe.h"

class FilterManager;
class FilterChain;

class Deinterlacer
{
public:
	Deinterlacer();
	~Deinterlacer();
	
	void processFrame(VideoFrame *frame);

private:
	void setup(VideoFrame *frame);
	
	QScopedPointer<FilterManager> m_manager;
	QScopedPointer<FilterChain> m_deint;
	QString m_filterName;
};

#endif
