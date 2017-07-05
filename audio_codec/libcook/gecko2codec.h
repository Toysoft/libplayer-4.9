/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Description: adpcm decoder
 */
/**************************************************************************************
 * Fixed-point RealAudio 8 decoder
 * Jon Recker (jrecker@real.com), Ken Cooke (kenc@real.com)
 * October 2003
 *
 * gecko2codec.h - public C API for Gecko2 decoder
 **************************************************************************************/

#ifndef _GECKO2CODEC_H
#define _GECKO2CODEC_H

//#include "includes.h"
#define _ARC32

#if defined(_WIN32) && !defined(_WIN32_WCE)

#elif defined(_WIN32) && defined(_WIN32_WCE) && defined(ARM)

#elif defined(_WIN32) && defined(WINCE_EMULATOR)

#elif defined(ARM_ADS)

#elif defined(_SYMBIAN) && defined(__WINS__)

#elif defined(__GNUC__) && defined(ARM)

#elif defined(__GNUC__) && defined(__i386__)

#elif defined(_OPENWAVE)

#elif defined(_ARC32)

#else
#error No platform defined. See valid options in gecko2codec.h.
#endif

#ifdef __cplusplus
extern "C" {
#endif

    typedef void *HGecko2Decoder;

    enum {
        ERR_GECKO2_NONE =                  0,
        ERR_GECKO2_INVALID_SIDEINFO =     -1,

        ERR_UNKNOWN =                  -9999
    };

    /* public API */
    HGecko2Decoder Gecko2InitDecoder(int nSamples, int nChannels, int nRegions, int nFrameBits, int sampRate, int cplStart, int cplQbits, int *codingDelay);
    void Gecko2FreeDecoder(HGecko2Decoder hGecko2Decoder);
    int Gecko2Decode(HGecko2Decoder hGecko2Decoder, unsigned char *codebuf, int lostflag, short *outbuf, unsigned timestamp);

#ifdef __cplusplus
}
#endif

#endif  /* _GECKO2CODEC_H */

