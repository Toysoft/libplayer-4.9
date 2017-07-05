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
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <audio-dec.h>
#include <adec-armdec-mgt.h>

#define AMFFMPEG_INFRAME_BUFSIZE 1024*8
#define AMFFMPEG_OUTFRAME_BUFSIZE 1024*128
#define	Clip(acc,min,max)	((acc) > max ? max : ((acc) < min ? min : (acc)))



AVCodecContext *ic;
AVCodec *codec;
unsigned char indata[8192];

typedef struct {
    int ValidDataLen;
    int UsedDataLen;
    unsigned char *BufStart;
    unsigned char *pcur;
} amffmpeg_read_ctl_t;




