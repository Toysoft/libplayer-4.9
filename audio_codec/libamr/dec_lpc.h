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
#ifndef DEC_LPC_H
#define DEC_LPC_H

#include "typedef.h"

void D_LPC_isf_noise_d(Word16 *indice, Word16 *isf_q);
void D_LPC_isf_isp_conversion(Word16 isf[], Word16 isp[], Word16 m);
void D_LPC_isp_a_conversion(Word16 isp[], Word16 a[], Word32 adaptive_scaling,
                            Word16 m);
void D_LPC_a_weight(Word16 a[], Word16 ap[], Word16 gamma, Word16 m);
void D_LPC_isf_2s3s_decode(Word16 *indice, Word16 *isf_q, Word16* past_isfq,
                           Word16 *isfold, Word16 *isf_buf, Word16 bfi);
void D_LPC_isf_2s5s_decode(Word16 *indice, Word16 *isf_q, Word16 *past_isfq,
                           Word16 *isfold, Word16 *isf_buf, Word16 bfi);
void D_LPC_int_isp_find(Word16 isp_old[], Word16 isp_new[],
                        const Word16 frac[], Word16 Az[]);
void D_LPC_isf_extrapolation(Word16 HfIsf[]);

#endif

