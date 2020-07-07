#ifndef MCF_CD3_AUDIO_DETECTOR_H_
#define MCF_CD3_AUDIO_DETECTOR_H_

#include <deque>

#include "audiooutput.h"

#include "FrameMetadata.h"

#ifndef CHANNELS_MAX
#define CHANNELS_MAX 8
#endif

struct AudioSample
{
    int64_t time;
    int numChannels;
    int16_t peak[CHANNELS_MAX+1];

    AudioSample()
    {
        time = 0;
        numChannels = 0;
        memset(peak, 0, sizeof(int16_t)*(CHANNELS_MAX+1));
    }
};

class AudioDetector : public AudioOutput
{
public:
    AudioDetector();
    virtual ~AudioDetector();

    void Enable(double fps);
    void processFrame(FrameMetadata &curFrame);

    // AudioOutput overrides:
    virtual void Reconfigure(const AudioSettings &settings);
    virtual AudioOutputSettings* GetOutputSettingsCleaned(bool digital = true);
    virtual AudioOutputSettings* GetOutputSettingsUsers(bool digital = true) { return GetOutputSettingsCleaned(digital); }
    virtual void Reset(void);
    virtual bool AddFrames(void *buffer, int frames, int64_t timecode);
    virtual bool AddData(void *buffer, int len, int64_t timecode, int frames);
    virtual void SetTimecode(int64_t timecode);
    virtual int64_t GetAudiotime(void);

    virtual void SetEffDsp(int) {}
    virtual bool IsPaused(void) const { return false; }
    virtual void Pause(bool paused) {}
    virtual void PauseUntilBuffered(void) {}
    virtual void Drain(void) {}
    virtual void bufferOutputData(bool) {}
    virtual int readOutputData(unsigned char *read_buffer, int max_length) { return 0; }
    virtual bool IsUpmixing(void)   { return false; }
    virtual bool ToggleUpmix(void)  { return false; }
    virtual bool CanUpmix(void)     { return false; }
    virtual bool CanProcess(AudioFormat fmt) { return fmt == FORMAT_S16 || fmt == FORMAT_FLT; }
    virtual uint32_t CanProcess(void) { return (1 << FORMAT_S16) | (1 << FORMAT_FLT); }
    virtual int GetVolumeChannel(int) const { return 100; }
    virtual void SetVolumeChannel(int, int) {}
    virtual void SetVolumeAll(int) {}
    virtual int GetSWVolume(void) { return 100; }
    virtual void SetSWVolume(int, bool) {}

private:
    bool m_enabled;
    double m_fps;
    int64_t m_last_timecode;
    AudioSettings m_settings;
    QScopedPointer<AudioOutputSettings> m_output_settings;
    QMutex m_mutex;
    std::deque<AudioSample> m_samples;
};

#endif//MCF_CD3_AUDIO_DETECTOR_H_
