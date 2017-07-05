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



#include "utils/Log.h"
#include <android/native_window.h>
#include "Amsufaceutils.h"

namespace android
{


int InitVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer)
{
    sp<ANativeWindow> mNativeWindow = NULL;
	if (bufferProducer == NULL) 
		return -1;
	mNativeWindow = new Surface(bufferProducer);
	///native_window_set_usage(mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_AML_VIDEO_OVERLAY);
	native_window_set_buffers_format(mNativeWindow.get(), WINDOW_FORMAT_RGBA_8888);
	native_window_set_scaling_mode(mNativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_FREEZE);
	return 0;
}

}
