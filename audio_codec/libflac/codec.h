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
#ifndef DSP_CODEC_HEADER
#define DSP_CODEC_HEADER
#include "codec_message.h"

struct codec_type {
    char *name;
    int (*init)(struct frame_fmt *);
    int (*release)(void);
    // return BYTEs number for the samples
    int (*decode_frame)(unsigned char *, int, struct frame_fmt *);
};

/*data in*/
int read_bits(int bits);
int bits_left(void);
int reset_bits(void);
int read_byte(void);
int read_buffer(unsigned char *buffer, int size);
int get_inbuf_data_size(void);
unsigned long  get_stream_in_offset(void);
int stream_in_buffer_init(void);

/*data out*/
int write_buffer(unsigned char *buf, int size);
int get_outbuf_space(void);
int out_buffer_init(void);

/*mgt*/
int codec_start(void);
int codec_resume(void);
int codec_pause(void);
int register_codec(const struct codec_type   *mc);
void decode_error_msg(int error);
void trans_err_code(error_code_t error);

#endif
