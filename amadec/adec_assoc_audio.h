/*
 * Copyright (C) 2010 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ADEC_ASSOC_AUDIO_H_
#define ADEC_ASSOC_AUDIO_H_

int InAssocBufferInit(aml_audio_dec_t *audec);
int InAssocBufferRelease(aml_audio_dec_t *audec);

void get_assoc_stream_info(buffer_stream_t *bs);
int set_assoc_enable(unsigned int enable);
int get_assoc_status(void);
int read_assoc_data(aml_audio_dec_t *audec, unsigned char *buffer, int size);
void audio_set_mixing_level_between_main_assoc(int mixing_level);

#endif

