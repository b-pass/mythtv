#ifndef _AUDIOBUFFER_H_
#define _AUDIOBUFFER_H_

#define USE_AUDIO_LIST

// POSIX headers
#include <stdint.h>

// Qt headers
#include <QObject>
#include <QSharedPointer>

// MythTV headers
#include "mythcontext.h"
#include <deque>

#include "audiooutput.h"

//#define AUDIODEBUGGING
//#define AUDIODEBUGGING2
//#define AUDIODEBUGGING3

class AudioSample : public QSharedData
{
public:
    int64_t time;
    int64_t duration;
    double  power;
    int     channels;
    bool    changed;
    AudioSample();
    QString toString() const;
};

typedef QExplicitlySharedDataPointer< AudioSample > PAudioSample;


#define CHANNELS_MIN 1
#define CHANNELS_MAX 8
class AudioBuffer : public AudioOutput
{
 public:
    AudioBuffer(const AudioSettings &settings);
   ~AudioBuffer();

    AudioOutputSettings* GetOutputSettings(bool /*digital*/);
    AudioOutputSettings* GetOutputSettingsUsers(bool digital);
    AudioOutputSettings* GetOutputSettingsCleaned(bool digital);

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &orig_settings);

    void Enable();

    virtual void SetEffDsp(int /* dsprate */);
    virtual void SetBlocking(bool block);
    virtual void Reset(void);
    virtual bool AddFrames(void *in_buffer, int in_frames, int64_t timecode);
    virtual bool AddData(void *in_buffer, int in_len,
                         int64_t timecode, int in_frames);
    const PAudioSample GetSample(int64_t time);
    virtual void SetTimecode(int64_t timecode);
    virtual bool GetPause(void);
    virtual void Pause(bool paused);
    virtual void Drain(void);
    virtual bool IsPaused() const;
    virtual void PauseUntilBuffered();
    virtual bool IsUpmixing();
    virtual bool ToggleUpmix();
    virtual bool CanUpmix();
    virtual bool CanProcess(AudioFormat fmt);
    virtual uint32_t CanProcess(void);

    virtual int64_t GetAudiotime(void);
    virtual int GetVolumeChannel(int) const;
    virtual void SetVolumeChannel(int, int);
    virtual void SetVolumeAll(int);
    virtual int GetSWVolume(void);
    virtual void SetSWVolume(int, bool);
    virtual uint GetCurrentVolume(void) const;
    virtual void SetCurrentVolume(int);
    virtual void AdjustCurrentVolume(int);
    virtual void SetMute(bool);
    virtual void ToggleMute(void);
    virtual MuteState GetMute(void);
    virtual MuteState IterateMutedChannels(void);

    //  These are pure virtual in AudioOutput, but we don't need them here
    virtual void bufferOutputData(bool);
    virtual int readOutputData(unsigned char*, int );

    // Basic details about the audio stream
    int channels;
    int codec;
    int bytes_per_frame;
    int output_bytes_per_frame;
    AudioFormat format; 
    AudioFormat output_format;
    int samplerate;
    int source_channels;
    int source_samplerate;
    int source_bytes_per_frame;

    int bufsize;
    //unsigned char *audiobuffer;
    //int audiobuffer_len;
    //int channels, bits, bytes_per_sample, eff_audiorate;
    int configured_channels;
    AudioSettings settings;
    int64_t last_audiotime;

    AudioOutputSettings* output_settings;

    bool changed;
    bool info_valid;

    int64_t first_audiotime;

#ifdef USE_AUDIO_LIST
    std::deque< PAudioSample > audioSamples;
#else
    std::vector< PAudioSample > audioSamples;
#endif

 private:
    mutable QMutex m_lock;
    bool verboseDebugging;
    bool enabled;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
