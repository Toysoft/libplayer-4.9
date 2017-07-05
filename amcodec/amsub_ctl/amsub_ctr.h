/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef CODEC_AMSUB_CTRL_H_
#define CODEC_AMSUB_CTRL_H_

void amsub_start(void **priv, amsub_info_t *amsub_info);
void amsub_stop(void **priv);
int amsub_outdata_read(void **priv, amsub_info_t *amsub_info);

#endif
