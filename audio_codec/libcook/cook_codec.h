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
#ifndef COOK_CODEC_HEADERS
#define COOK_CODEC_HEADERS


#define SUB_FMT_VALID           (1<<1)
#define CHANNEL_VALID           (1<<2)
#define SAMPLE_RATE_VALID       (1<<3)
#define DATA_WIDTH_VALID        (1<<4)

#define AUDIO_EXTRA_DATA_SIZE  (2048*4)

struct audio_info {
    int valid;
    int sample_rate;
    int channels;
    int bitrate;
    int codec_id;
    int block_align;
    int extradata_size;
    char extradata[AUDIO_EXTRA_DATA_SIZE];
};
#if 0
typedef enum {
    DECODE_INIT_ERR,
    DECODE_FATAL_ERR,
} err_code_t;
#endif

#endif

