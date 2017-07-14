#include "FrameMetadataAggregator.h"
#include "AudioSample.h"
#include "AudioDetector.h"

AudioDetector::AudioDetector()
    : m_enabled(false)
    , m_last_timecode(0)
{
}

AudioDetector::~AudioDetector()
{
}

void AudioDetector::Enable()
{
    m_enabled = true;
    Reset();
}

void AudioDetector::Reconfigure(const AudioSettings &settings)
{
    ClearError();

    m_settings = settings;
    
    {
        QMutexLocker locker(&m_mutex);
        m_samples.clear();
    }
}

AudioOutputSettings* AudioDetector::GetOutputSettingsCleaned(bool digital)
{
    if (!m_output_settings)
    {
        m_output_settings.reset(new AudioOutputSettings());
        m_output_settings->AddSupportedFormat(FORMAT_S16);
        for (uint c = 0; c < CHANNELS_MAX; c++)
            m_output_settings->AddSupportedChannels(c);
    }
    return m_output_settings.data();
}

void AudioDetector::Reset(void)
{
    m_last_timecode = 0;
    
    {
        QMutexLocker locker(&m_mutex);
        m_samples.clear();
    }
}

void AudioDetector::SetTimecode(int64_t timecode)
{
    m_last_timecode = timecode;
}

int64_t AudioDetector::GetAudiotime(void)
{
    return m_last_timecode;
}

bool AudioDetector::AddFrames(void *buffer, int frames, int64_t timecode)
{
    return AddData(buffer, frames * m_settings.channels * AudioOutputSettings::SampleSize(m_settings.format), timecode, frames);
}

bool AudioDetector::AddData(void *buffer, int len, int64_t timecode, int frames)
{
    m_last_timecode = timecode;
    if (!m_enabled)
        return true;

    QSharedPointer<AudioSample> as(new AudioSample);
    as->time = timecode;
    as->duration = (frames * 1000) / m_settings.samplerate;
    as->num_channels = std::max(0, std::min(m_settings.channels, CHANNELS_MAX));

    if (m_settings.format == FORMAT_FLT)
    {
        float *flt = (float*)buffer;
        for (int f = 0; f < frames; ++f)
        {
            for (int c = 0; c < as->num_channels; c++)
                as->peak[c] = std::max(as->peak[c], (int16_t)std::abs(*flt++ * 32000.0f));
        }
    }
    else
    {
        int16_t *s16 = (int16_t *)buffer;
        for (int f = 0; f < frames; ++f)
        {
            for (int c = 0; c < as->num_channels; c++)
            {
                int16_t s = *s16++;
                if (s < 0)
                    s = -s;
                as->peak[c] = std::max(as->peak[c], s);
            }
        }
    }
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_samples.empty() && as->time < m_samples.back()->time)
            m_samples.clear();
        m_samples.push_back(as);
    }
    
    return true;
}

void AudioDetector::processMore(FrameMetadata const &curFrame, FrameMetadataAggregator *agg)
{
    while (true)
    {
        QSharedPointer<AudioSample> sample;
        {
            QMutexLocker locker(&m_mutex);
            if (m_samples.empty())
                return;
            sample = m_samples.front();
            m_samples.pop_front();
        }
        if (!agg->addAudio(sample.data()))
        {
            QMutexLocker locker(&m_mutex);
            m_samples.push_front(sample);
            break;
        }
    }
}
