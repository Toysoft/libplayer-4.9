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

#ifndef _DSP_TYPES_H
#define _DSP_TYPES_H
/* bsd */
typedef unsigned char       u_char;
typedef unsigned short      u_short;
typedef unsigned int        u_int;
typedef unsigned long       u_long;

/* sysv */
typedef unsigned char       unchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;

typedef unsigned char       __u8;
typedef unsigned short  __u16;
typedef unsigned int        __u32;
typedef unsigned long long      __u64;


typedef  signed char        __s8;
typedef  short  __s16;
typedef  int        __s32;
typedef  long long      __s64;

typedef     __u8        u_int8_t;
typedef     __s8        int8_t;
typedef     __u16       u_int16_t;
typedef     __s16       int16_t;
typedef     __u32       u_int32_t;
typedef     __s32       int32_t;


typedef     __u8        uint8_t;
typedef     __u16       uint16_t;
typedef     __u32       uint32_t;

typedef     __u64       uint64_t;
typedef     __u64       u_int64_t;
typedef     __s64       int64_t;
typedef     float       float32_t;


#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
