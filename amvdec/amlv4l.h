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



#ifndef AMLV4L_HEAD_A__
#define AMLV4L_HEAD_A__
#include <amvideo.h>

typedef struct amlv4l_dev {
    int v4l_fd;
    unsigned int bufferNum;
    vframebuf_t *vframe;
    int type;
    int width;
    int height;
    int pixformat;
    int memory_mode;
} amlv4l_dev_t;
amvideo_dev_t *new_amlv4l(void);
int amlv4l_release(amvideo_dev_t *dev);

#endif//AMLV4L_HEAD_A__