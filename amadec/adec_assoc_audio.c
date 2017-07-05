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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <adec_write.h>
#include <Amsysfsutils.h>
#include <audio-dec.h>
#ifdef ANDROID
#include <cutils/properties.h>
#endif
#include <adec_write.h>

static char UDCMixingLevel[] = "media.udc.mixinglevel";


int InAssocBufferInit(aml_audio_dec_t *audec)
{
    audec->g_assoc_bst = malloc(sizeof(buffer_stream_t));
    if (!audec->g_assoc_bst) {
        adec_print("[%s %d]g_bst malloc failed! \n", __FUNCTION__, __LINE__);
        audec->g_assoc_bst = NULL;
        return -1;
    } else {
        adec_print("[%s %d] audec->g_bst/%p", __FUNCTION__, __LINE__, audec->g_assoc_bst);
    }

    memset(audec->g_assoc_bst, 0, sizeof(buffer_stream_t));

    audec->adec_ops->nInAssocBufSize = DEFAULT_ASSOC_AUDIO_BUFFER_SIZE;

    int ret = init_buff(audec->g_assoc_bst, audec->adec_ops->nInAssocBufSize);
    if (ret == -1) {
        adec_print("[%s %d]pcm buffer init failed !\n", __FUNCTION__, __LINE__);
        return -1;
    }
    adec_print("[%s %d]pcm buffer init ok buf_size:%d\n", __FUNCTION__, __LINE__, audec->g_assoc_bst->buf_length);

    audec->g_assoc_bst->data_width = audec->data_width = AV_SAMPLE_FMT_S16;

    if (audec->channels > 0) {
        audec->g_assoc_bst->channels = audec->channels;
    }
    if (audec->samplerate > 0) {
        audec->g_assoc_bst->samplerate = audec->samplerate;
    }
    audec->g_assoc_bst->format = audec->format;

    return 0;
}

int InAssocBufferRelease(aml_audio_dec_t *audec)
{
    if (audec->g_assoc_bst) {
        adec_print("[%s %d] audec->g_bst/%p", __FUNCTION__, __LINE__, audec->g_assoc_bst);
        release_buffer(audec->g_assoc_bst);
        audec->g_assoc_bst = NULL;
    }
    return 0;
}

//To get the associate data if assoc is able
int read_assoc_data(aml_audio_dec_t *audec, unsigned char *buffer, int size)
{
    int ret = 0;

    if ((audec->g_assoc_bst != NULL) && (audec->associate_audio_enable == 1)) {
        ret = read_es_buffer((char *)buffer, audec->g_assoc_bst, size);
    }
    else {
        adec_print("[%s]-[assoc_enable:%d]-[assoc_bst_ptr:%p]\n",
        __FUNCTION__, audec->associate_audio_enable, audec->g_assoc_bst);
    }

    return ret;
}

//To set the mixing level between main and associate
void audio_set_mixing_level_between_main_assoc(int mixing_level)
{
    char buffer[16] = {0};
    int mix_user_prefer = 0;

    if (mixing_level < 0) {
        mixing_level = 0;
    }
    else if (mixing_level > 100) {
        mixing_level = 100;
    }
    mix_user_prefer = (mixing_level*64 -32*100)/100;//[0,100] mapping to [-32,32]
    sprintf(buffer, "%d", mix_user_prefer);
    adec_print("%s-[mixing_level:%d]-[mix_user_prefer:%d]-[buffer:%s]", __FUNCTION__, mixing_level, mix_user_prefer, buffer);
    amsysfs_write_prop(UDCMixingLevel,buffer);

    return ;
}



