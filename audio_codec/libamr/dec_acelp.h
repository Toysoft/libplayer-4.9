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
/*
 *===================================================================
 *  3GPP AMR Wideband Floating-point Speech Codec
 *===================================================================
 */
#ifndef DEC_ACELP_H
#define DEC_ACELP_H

#include "typedef.h"

void D_ACELP_decode_2t(Word16 index, Word16 code[]);
void D_ACELP_decode_4t(Word16 index[], Word16 nbbits, Word16 code[]);
void D_ACELP_phase_dispersion(Word16 gain_code, Word16 gain_pit, Word16 code[],
                              Word16 mode, Word16 disp_mem[]);

#endif

