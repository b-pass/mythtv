#include <unistd.h>

#include "AudioDetector.h"

AudioDetector::AudioDetector()
    : m_enabled(false)
    , m_fps(1)
    , m_last_timecode(0)
{
}

AudioDetector::~AudioDetector()
{
}

void AudioDetector::Enable(double fps)
{
    m_enabled = true;
    m_fps = fps;
    if (m_fps == 0)
        m_fps = 1; // divide by zero is bad mmmkay
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
        m_output_settings->AddSupportedFormat(FORMAT_FLT);
        for (uint c = 1; c <= CHANNELS_MAX; c++)
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
    return AddData(buffer, frames * m_settings.m_channels * AudioOutputSettings::SampleSize(m_settings.m_format), timecode, frames);
}

bool AudioDetector::AddData(void *buffer, int len, int64_t timecode, int frames)
{
    m_last_timecode = timecode;
    if (!m_enabled)
        return true;

    int frames_per = m_settings.m_sampleRate / m_fps / 10 + 1;
    if (frames_per < 1)
        frames_per = 1;
    float *flt = (float*)buffer;
    int16_t *s16 = (int16_t *)buffer;

    int f = 0;
    while (f < frames)
    {
        AudioSample as;
        as.time = timecode + (f * 1000) / m_settings.m_sampleRate;
        as.numChannels = std::max(0, std::min(m_settings.m_channels, CHANNELS_MAX));

        if (m_settings.m_format == FORMAT_FLT)
        {
            for (int j = 0; j < frames_per; ++j)
            {
                for (int c = 0; c < as.numChannels; c++)
                {
                    int16_t s = (int16_t)std::abs(*flt++ * 32000.0f);
                    as.peak[c] = std::max(as.peak[c], s);
                }
            }
        }
        else
        {
            for (int j = 0; j < frames_per; ++j)
            {
                for (int c = 0; c < as.numChannels; c++)
                {
                    int16_t s = *s16++;
                    if (s < 0)
                        s = -s;
                    as.peak[c] = std::max(as.peak[c], s);
                }
            }
        }
        
        {
            QMutexLocker locker(&m_mutex);
            if (!m_samples.empty() && as.time < m_samples.back().time)
                m_samples.clear();
            m_samples.push_back(as);
        }

        f += frames_per;
    }
    
    return true;
}

void AudioDetector::processFrame(FrameMetadata &curFrame)
{
    size_t empty = 0;
    while (true)
    {
        if (empty)
        {
            if (empty > 100)
                break;
            usleep(10000);
        }
        AudioSample sample;
        {
            QMutexLocker locker(&m_mutex);
            if (m_samples.empty())
            {
                ++empty;
                continue;
            }
            empty = 0;
            sample = m_samples.front();
            m_samples.pop_front();
        }

        if (sample.time <= curFrame.timeCode + int64_t(1/m_fps * 1000))
        {
            // sample is before the end of this frame, use it
            curFrame.numChannels = std::max(curFrame.numChannels, sample.numChannels);
            for (int c = 0; c < sample.numChannels; c++)
                curFrame.peakAudio[c] = std::max(curFrame.peakAudio[c], sample.peak[c]);
        }
        else
        {
            // sample goes after this frame, save it for later
            QMutexLocker locker(&m_mutex);
            m_samples.push_front(sample);
            break;
        }
    }
}
