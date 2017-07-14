#ifndef MCF_CD3_AUDIO_SAMPLE_H_
#define MCF_CD3_AUDIO_SAMPLE_H_

#include <cstdint>

#ifndef CHANNELS_MAX
#define CHANNELS_MAX 8
#endif

struct AudioSample
{
    int64_t time;
    uint64_t duration;
    int num_channels;
    int16_t peak[CHANNELS_MAX+1];

    AudioSample()
    {
        time = 0;
        duration = 0;
        num_channels = 0;
        memset(peak, 0, sizeof(int16_t)*(CHANNELS_MAX+1));
    }
};

#endif//MCF_CD3_AUDIO_SAMPLE_H_
