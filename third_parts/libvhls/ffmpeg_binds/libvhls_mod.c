/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


//by peter,2012,1121
#include "ammodule.h"
#include "libavformat/url.h"
#include "libavformat/avformat.h"
#ifdef ANDROID
#include <android/log.h>
#define  LOG_TAG    "libvhls_mod"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI printf
#endif

ammodule_methods_t  libvhls_module_methods;
ammodule_t AMPLAYER_MODULE_INFO_SYM = {
tag:
    AMPLAYER_MODULE_TAG,
version_major:
    AMPLAYER_API_MAIOR,
version_minor:
    AMPLAYER_API_MINOR,
    id: 0,
name: "vhls_mod"
    ,
author: "Amlogic"
    ,
descript: "libvhls module binding library"
    ,
methods:
    &libvhls_module_methods,
dso :
    NULL,
reserved :
    {0},
};

extern URLProtocol vhls_protocol;
extern URLProtocol vrwc_protocol;
extern AVInputFormat ff_mhls_demuxer; // hls demuxer for media group

int libvhls_mod_init(const struct ammodule_t* module, int flags)
{
    LOGI("libvhls module init\n");
    av_register_protocol(&vhls_protocol);
    av_register_protocol(&vrwc_protocol);//add for verimatrix drm link
    av_register_input_format(&ff_mhls_demuxer);
    return 0;
}


int libvhls_mod_release(const struct ammodule_t* module)
{
    LOGI("libvhls module release\n");
    return 0;
}


ammodule_methods_t  libvhls_module_methods = {
    .init =  libvhls_mod_init,
    .release =   libvhls_mod_release,
} ;

