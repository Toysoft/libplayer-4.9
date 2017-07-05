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



#ifndef AMVDEC_PRIV_HEADER__
#define AMVDEC_PRIV_HEADER__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <linux/videodev2.h>
#include <android/log.h>

#define  TAG        "amvdec"
#define  XLOG(V,T,...)  __android_log_print(V,T,__VA_ARGS__)
#define  LOGI(...)   XLOG(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define  LOGE(...)   XLOG(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)


#endif