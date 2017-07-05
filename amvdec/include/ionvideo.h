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



#ifndef IONVDEC_AMVIDEO_HEADER_SS
#define  IONVDEC_AMVIDEO_HEADER_SS
#include <stdlib.h>
//#include <linux/videodev2.h>
#include "videodev2.h"
#define FLAGS_OVERLAY_MODE (1)
#define FLAGS_V4L_MODE   (2)

struct ionvideo_dev;
typedef struct vframebuf {
    char * vbuf;
    int fd;
    int index;
    int offset;
    int length;
    int64_t pts;
    int duration;
} vframebuf_t;

struct ionvideo_dev_ops {
    int (*setparameters)(struct ionvideo_dev *dev, int cmd, void*para);
    int (*getparameters)(struct ionvideo_dev *dev, struct v4l2_format *fmt);
    int (*init)(struct ionvideo_dev *dev, int flags, int width, int height, int fmt, int buffernum);
    int (*release)(struct ionvideo_dev *dev);
    int (*dequeuebuf)(struct ionvideo_dev *dev, vframebuf_t*vf);
    int (*queuebuf)(struct ionvideo_dev *dev, vframebuf_t*vf);
    int (*start)(struct ionvideo_dev *dev);
    int (*stop)(struct ionvideo_dev *dev);
};

typedef struct ionvideo_dev {
    char devname[8];
    int mode;
    struct ionvideo_dev_ops ops;
    void *devpriv;
} ionvideo_dev_t;

typedef struct ionvideo {
    ionvideo_dev_t *dev;
} ionvideo_t;

ionvideo_dev_t *new_ionvideo(int flags);
int ionvideo_setparameters(ionvideo_dev_t *dev, int cmd, void * parameters);
int ionvideo_getparameters(ionvideo_dev_t *dev, int *width, int *height, int *pixelformat);
int ionvideo_init(ionvideo_dev_t *dev, int flags, int width, int height, int fmt, int buffernum);
int ionvideo_start(ionvideo_dev_t *dev);
int ionvideo_stop(ionvideo_dev_t *dev);
int ionvideo_release(ionvideo_dev_t *dev);
int ionv4l_dequeuebuf(ionvideo_dev_t *dev, vframebuf_t*vf);
int ionv4l_queuebuf(ionvideo_dev_t *dev, vframebuf_t*vf);
#endif
