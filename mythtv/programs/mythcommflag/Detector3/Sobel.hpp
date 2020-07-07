#ifndef MCF_CD3_SOBEL_HPP_
#define MCF_CD3_SOBEL_HPP_

#include <stdint.h>
#include <cmath>

// http://en.wikipedia.org/wiki/Sobel_operator

inline bool IsSobelEdgeAt(uint8_t const *buf,
							uint32_t x,
							uint32_t y,
							uint32_t stride,
							uint8_t threshold = 40)
{
    uint32_t center = y * stride + x;
    uint32_t above = center - stride;
    uint32_t below = center + stride;
    
    int32_t gx = 0;
    gx += buf[above - 1];
    gx += (buf[center - 1]*2);
    gx += buf[below - 1];
    
    gx -= buf[above + 1];
    gx -= (buf[center + 1]*2);
    gx -= buf[below + 1];
    
    int32_t gy = 0;
    gy += buf[above - 1];
    gy += (buf[above]*2);
    gy += buf[above + 1];
    
    gy -= buf[below - 1];
    gy -= (buf[below]*2);
    gy -= buf[below + 1];
    
    return (abs(gx) + abs(gy)) >= (3*threshold);
}

#endif//MCF_CD3_SOBEL_HPP_
