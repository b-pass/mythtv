
#include "AudioBuffer.h"

#define USE_PEAK 0

AudioSample::AudioSample()
{
    time = 0;
    duration = 0;
    power = 0;
    channels = 0;
    changed = false;
}

QString AudioSample::toString() const
{
    return QString("%1 %2 %3 %4 %5")
        .arg(time)
        .arg(duration)
        .arg(power)
        .arg(channels)
        .arg(changed);
}


AudioBuffer::AudioBuffer(const AudioSettings &settings) :
    configured_channels(CHANNELS_MAX),
    last_audiotime(0),
    output_settings(0),
    changed(false),
    info_valid(false),
    m_lock(),
    verboseDebugging(false),
    enabled(false)
{
    if (getenv("DEBUGCOMMFLAG"))
        verboseDebugging = true;

    Reset();
    //AudioSettings orig_settings("", "", AudioFormat::,);
    //Reconfigure(audio_bits, audio_channels, 0);
    Reconfigure(settings);
}

AudioBuffer::~AudioBuffer()
{
    //delete [] audiobuffer;
    if (output_settings)
    {
        delete output_settings;
        output_settings = NULL;
    }
}

AudioOutputSettings* AudioBuffer::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
    {
        settings->AddSupportedChannels(channels);
    }
    settings->AddSupportedFormat(FORMAT_S16);
    settings->AddSupportedFormat(FORMAT_FLT);

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("GetOutputSettings"));
    return settings;
}

AudioOutputSettings* AudioBuffer::GetOutputSettingsUsers(bool digital)
{
    if (!output_settings)
        output_settings = GetOutputSettings(digital);
    return output_settings;
}

AudioOutputSettings* AudioBuffer::GetOutputSettingsCleaned(bool digital)
{
    if (!output_settings)
        output_settings = GetOutputSettings(digital);
    return output_settings;
}

void AudioBuffer::Enable()
{
    enabled = true;
}

// reconfigure sound out for new params
void AudioBuffer::Reconfigure(const AudioSettings &orig_settings)
{
    ClearError();
    changed = (orig_settings.format != settings.format)
        | (orig_settings.channels != settings.channels)
        | (orig_settings.samplerate != settings.samplerate);
    settings = orig_settings;

    source_bytes_per_frame = source_channels * AudioOutputSettings::SampleSize(settings.format);
    LOG(VB_COMMFLAG, LOG_INFO,
        QString("Audio Reconfigure changed %1, Settings fmt=%2 ch=%3 sr=%4.")
                .arg(changed)
                .arg(settings.format)
                .arg(settings.channels)
                .arg(settings.samplerate));

    QMutexLocker locker(&m_lock);
    audioSamples.clear();
    last_audiotime = 0;
}

bool AudioBuffer::CanProcess(AudioFormat fmt)
{
    return fmt == FORMAT_S16 || fmt == FORMAT_FLT;
}

uint32_t AudioBuffer::CanProcess(void)
{
    return (1 << FORMAT_S16) | (1 << FORMAT_FLT);
}

// dsprate is in 100 * samples/second
void AudioBuffer::SetEffDsp(int /* dsprate */)
{
    //eff_audiorate = (dsprate / 100);
}

void AudioBuffer::SetBlocking(bool block) 
{
    (void)block; 
}

void AudioBuffer::Reset(void)
{
    if (!enabled)
        return;
    if (verboseDebugging)
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
            QString("audio reset"));
    }
    QMutexLocker locker(&m_lock);
    audioSamples.clear();
    last_audiotime = 0;
}

bool AudioBuffer::AddFrames(void *in_buffer, int in_frames,
                                        int64_t timecode)
{
    if (!enabled)
    {
        last_audiotime = timecode;
        return true;
    }
    return AddData(in_buffer, in_frames * source_bytes_per_frame, timecode,
                                   in_frames);
}

bool AudioBuffer::AddData(void *in_buffer, int in_len,
                     int64_t timecode, int in_frames)
{
    if (!enabled)
    {
        last_audiotime = timecode;
        return true;
    }
    int i;
    int f;
    PAudioSample aSample(new AudioSample);
    aSample->power = 0;
    aSample->time = timecode;    // in ms
    aSample->duration = (in_frames * 1000) / settings.samplerate; // in ms
    aSample->channels = settings.channels;
    if (changed)
    {
        aSample->changed = changed;
        changed = false;
    }
    if (timecode < last_audiotime)
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
            QString("audio time reset %1").arg(timecode));
        QMutexLocker locker(&m_lock);
        audioSamples.clear();
        last_audiotime = 0;
    }
    if (last_audiotime != 0)
    {
        int64_t nextAudiotime = last_audiotime + aSample->duration;
        while (aSample->time < nextAudiotime)
        {
            PAudioSample nullSample(new AudioSample);
            nullSample->power = -1;
            nullSample->time = nextAudiotime;
            nullSample->duration = aSample->duration;
            {
                QMutexLocker locker(&m_lock);
                audioSamples.push_back(nullSample);
            }
#ifdef AUDIODEBUGGING
            LOG(VB_COMMFLAG, LOG_DEBUG,
                QString("Audio AddData invalid time %1").arg(nullSample->time));
#endif
        }
    }
    switch (settings.format)
    {
        case FORMAT_S16:
            {
                int64_t power = 0;
                int16_t *p = (int16_t*)in_buffer;
                for(f=in_frames;f>0;--f)
                {
#if USE_PEAK
                    for(i=settings.channels; i>0; --i)
                    {
                        int32_t v = *p++;
                        if (v > power)
                        {
                            power = v;
                        }
                        else if (-v > power)
                        {
                            power = -v;
                        }
                    }
#else
                    // power as mono
                    int64_t sum = 0;
                    for(i=settings.channels; i>0; --i)
                    {
                        int32_t v = *p++;
                        sum += v;
                        //p++;
                    }
                    power += sum * sum;
#endif
                }
#if USE_PEAK
                aSample->power += ((double)power)/(INT16_MAX);
#else
                aSample->power += sqrt(((double)power)/(INT16_MAX * INT16_MAX)/in_frames);
#endif
                info_valid = true;
                QMutexLocker locker(&m_lock);
                audioSamples.push_back(aSample);
            } break;
        case FORMAT_FLT:
            {
                double power = 0;
                float *p = (float*)in_buffer;
#if USE_PEAK
                for(f=in_frames;f>0;--f)
                {
                    float v = *p++;
                    if (v > power)
                    {
                        power = v;
                    }
                    else if (-v > power)
                    {
                        power = -v;
                    }
                }
#else
                double ss[8];
                double s[8];
                for(i=0; i < settings.channels; i++)
                {
                    ss[i] = 0;
                    s[i] = 0;
                }
                for(f=in_frames;f>0;--f)
                {
                    // discrete power with dc cancelled using stddev calc
                    for(i=0; i < settings.channels; i++)
                    {
                        float v = *p++;
                        s[i] += v;
                        ss[i] += v * v;
                    }
                }
                for(i=0; i < settings.channels; i++)
                {
                    power += (ss[i] - (s[i] * s[i] / in_frames)) / in_frames;
                }
                power += sqrt(power);
#endif
                aSample->power += power;
                info_valid = true;
                QMutexLocker locker(&m_lock);
                audioSamples.push_back(aSample);
            } break;
        default:
            break;
    }
#ifdef AUDIODEBUGGING
    if (info_valid)
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
            QString("Audio AddData time %1 frames %2 power %3 changed %4").arg(timecode).arg(in_frames).arg(aSample->power).arg(aSample->changed));
    }
    else
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
            QString("Audio AddData time %1 frames %2").arg(timecode).arg(in_frames));
    }
#endif
    last_audiotime = timecode;
    return true;
}

const PAudioSample AudioBuffer::GetSample(int64_t time)
{
    if (!audioSamples.empty())
    {
        QMutexLocker locker(&m_lock);
#ifdef USE_AUDIO_LIST
        PAudioSample s;
        do
        {
            if (audioSamples.empty())
                break;
            s = audioSamples.front();
            if (!s)
                break;
            int64_t dtime = s->time - time;
            if (dtime < 0)
            {
                // time already past
#ifdef AUDIODEBUGGING2
                LOG(VB_COMMFLAG, LOG_DEBUG, QString("Audio GetSample past time %1 atime %2 duration %3 size %4").arg(time).arg(s->time).arg(s->duration).arg(audioSamples.size()));
#endif
                audioSamples.pop_front();
                continue;
            }
            if (dtime < s->duration)
            {
#ifdef AUDIODEBUGGING2
                LOG(VB_COMMFLAG, LOG_DEBUG, QString("Audio GetSample time %1 atime %2 duration %3 size %4").arg(time).arg(s->time).arg(s->duration).arg(audioSamples.size()));
#endif
                //audioSamples.pop_front();
                return s;
            }
            // not yet
            break;
        } while (true);
#else
        int64_t index = (time - audioSamples[0]->time)/audioSamples[0]->duration;
        //LOG(VB_COMMFLAG, LOG_DEBUG, QString("Audio GetSample time %1 tine0 %2 duration %3 index %4 size %5").arg(time).arg(audioSamples[0].time).arg(audioSamples[0].duration).arg(index).arg(audioSamples.size()));
        if (index >= 0 && index < audioSamples.size())
            return audioSamples[index];
#endif
    }
    return PAudioSample();
}

void AudioBuffer::SetTimecode(int64_t timecode)
{
    last_audiotime = timecode;
}
bool AudioBuffer::GetPause(void)
{
    return false;
}
void AudioBuffer::Pause(bool paused)
{
    (void)paused;
}
void AudioBuffer::Drain(void)
{
    // Do nothing
    return;
}
bool AudioBuffer::IsPaused() const { return false; }
void AudioBuffer::PauseUntilBuffered() { }
bool AudioBuffer::IsUpmixing() { return false; }
bool AudioBuffer::ToggleUpmix() { return false; }
bool AudioBuffer::CanUpmix() { return false; }


int64_t AudioBuffer::GetAudiotime(void)
{
    return last_audiotime;
}

int AudioBuffer::GetVolumeChannel(int) const
{ 
    // Do nothing
    return 100;
}
void AudioBuffer::SetVolumeChannel(int, int) 
{
    // Do nothing
}
void AudioBuffer::SetVolumeAll(int)
{
    // Do nothing
}
int AudioBuffer::GetSWVolume(void)
{
    return 100;
}
void AudioBuffer::SetSWVolume(int, bool) {}


uint AudioBuffer::GetCurrentVolume(void) const
{ 
    // Do nothing
    return 100;
}
void AudioBuffer::SetCurrentVolume(int) 
{
    // Do nothing
}
void AudioBuffer::AdjustCurrentVolume(int) 
{
    // Do nothing
}
void AudioBuffer::SetMute(bool) 
{
    // Do nothing
}
void AudioBuffer::ToggleMute(void) 
{
    // Do nothing
}
MuteState AudioBuffer::GetMute(void) 
{
    // Do nothing
    return kMuteOff;
}
MuteState AudioBuffer::IterateMutedChannels(void) 
{
    // Do nothing
    return kMuteOff;
}

//  These are pure virtual in AudioOutput, but we don't need them here
void AudioBuffer::bufferOutputData(bool){ return; }
int AudioBuffer::readOutputData(unsigned char*, int ){ return 0; }

/* vim: set expandtab tabstop=4 shiftwidth=4: */
