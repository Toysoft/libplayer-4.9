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



#ifndef IONV4L_HEAD_A__
#define IONV4L_HEAD_A__
#include <ionvideo.h>

typedef struct ionv4l_dev {
    int v4l_fd;
    unsigned int buffer_num;
    vframebuf_t *vframe;
    int type;
    int width;
    int height;
    int pixformat;
    int memory_mode;
} ionv4l_dev_t;
ionvideo_dev_t *new_ionv4l(void);
int ionv4l_release(ionvideo_dev_t *dev);

#endif//IONV4L_HEAD_A__
