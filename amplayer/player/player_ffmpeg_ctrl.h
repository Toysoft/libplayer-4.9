/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#ifndef _PLAYER_FFMPEG_CTRL_H_
#define _PLAYER_FFMPEG_CTRL_H_

#include "player_priv.h"

int ffmpeg_init(void);

int ffmpeg_open_file(play_para_t *am_p);
int ffmpeg_parse_file(play_para_t *am_p);
int ffmpeg_parse_file_type(play_para_t *am_p, player_file_type_t *type);
int ffmpeg_close_file(play_para_t *am_p);

void ffmpeg_interrupt(pthread_t thread_id);
void ffmpeg_uninterrupt(pthread_t thread_id);
void ffmpeg_interrupt_light(pthread_t thread_id);
void ffmpeg_uninterrupt_light(pthread_t thread_id);

int ffmpeg_buffering_data(play_para_t *para);
int ffmpeg_seturl_buffered_level(play_para_t *para, int levelx1000);
int ffmepg_seturl_codec_buf_info(play_para_t *para, int type, int value);
int ffmpeg_geturl_netstream_info(play_para_t* para, int type, void* value);

// for hls demuxer
int ffmpeg_set_format_codec_buffer_info(play_para_t * para, int type, int64_t value);
#endif

