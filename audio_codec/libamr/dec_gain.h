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
#ifndef DEC_GAIN_H
#define DEC_GAIN_H

#include "typedef.h"

void D_GAIN_init(Word16 *mem);
void D_GAIN_decode(Word16 index, Word16 nbits, Word16 code[], Word16 *gain_pit,
                   Word32 *gain_cod, Word16 bfi, Word16 prev_bfi,
                   Word16 state, Word16 unusable_frame, Word16 vad_hist,
                   Word16 *mem);
void D_GAIN_adaptive_control(Word16 *sig_in, Word16 *sig_out, Word16 l_trm);
void D_GAIN_lag_concealment_init(Word16 lag_hist[]);
void D_GAIN_lag_concealment(Word16 gain_hist[], Word16 lag_hist[], Word32 *T0,
                            Word16 *old_T0, Word16 *seed,
                            Word16 unusable_frame);
void D_GAIN_adaptive_codebook_excitation(Word16 exc[], Word32 T0, Word32 frac);
void D_GAIN_pitch_sharpening(Word16 *x, Word32 pit_lag, Word16 sharp);
Word16 D_GAIN_find_voice_factor(Word16 exc[], Word16 Q_exc, Word16 gain_pit,
                                Word16 code[], Word16 gain_code,
                                Word16 L_subfr);

#endif

