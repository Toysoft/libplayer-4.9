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
#ifndef _AUDIO_CODEC_RA_H
#define _AUDIO_CODEC_RA_H

#include "rm_parse.h"
#include "ra_depack.h"
#include "ra_decode.h"


typedef struct {
    ra_decode*          pDecode;
    ra_format_info*     pRaInfo;
    BYTE*               pOutBuf;
    UINT32              ulOutBufSize;
    UINT32              ulTotalSample;
    UINT32              ulTotalSamplePlayed;
    UINT32              ulStatus;
    UINT32              input_buffer_size;
    BYTE*               input_buf;
    UINT32              decoded_size;

} ra_decoder_info_t;

typedef enum {
    DECODE_INIT_ERR,
    DECODE_FATAL_ERR,
} error_code_t;


#define RADEC_IDLE  0
#define RADEC_INIT  1
#define RADEC_PLAY  2
#define RADEC_PAUSE 3

#define USE_C_DECODER
#endif
