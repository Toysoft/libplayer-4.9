LOCAL_PATH=$(shell pwd)
M_PREFIX=$(LIBPLAYER_STAGING_DIR)/usr
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include
INSTALL_DIR=${LIB_DIR}/libplayer/
LIBMOD_DIR=$(TARGET_DIR)/usr/lib/amplayer

#CROSS=$(BUILD_OUT_DIR)/host/opt/ext-toolchain/bin/arm-linux-gnueabihf-
#CC=$(CROSS)gcc
#CPP=$(CROSS)g++
#CXX=$(TARGET_CXX)
CC_SRC += cmf_main/hls_cmf_impl.c \
	  common/hls_rand.c	\
	  common/hls_utils.c	\
	  downloader/hls_download.c	\
	  downloader/hls_bandwidth_measure.c	\
	  ffmpeg_binds/ffmpeg_hls_wrapper.c	\
	  ffmpeg_binds/ffmpeg_hls_demuxer.c	\
	  ffmpeg_binds/libvhls_mod.c	\
	  hls_main/hls_fifo.c	\
	  hls_main/hls_m3ulivesession.c	\
	  hls_main/hls_m3uparser.c	\
	  hls_main/hls_simple_cache.c

CC_OBJS += $(patsubst %.c, %.o, $(CC_SRC))
CXX_OBJS += $(patsubst %.cpp, %.o, $(CXX_SRC))

TARGET_OBJS += $(CC_OBJS) $(CXX_OBJS)

TARGET=libvhls_mod.so

CFLAGS=-O2 -g  -I$(LOCAL_PATH)/include -I$(LOCAL_PATH)/hls_main -I$(LOCAL_PATH)/common -I$(LOCAL_PATH)/downloader -fPIC
CFLAGS+= -I$(LOCAL_PATH)/../../amffmpeg
CFLAGS+= -I$(LOCAL_PATH)/../../amavutils/include
CFLAGS+= -I$(LOCAL_PATH)/../../../../aml_libs/src/amavutils/include

#export CC CPP CFLAGS

LDFLAGS += -shared -L$(STAGING_DIR)/usr/lib/libvrwebclientDEV   -lstdc++

$(CC_OBJS): %.o: %.c
	$(CC) $(CFLAGS)  -c $< -o $@
$(CXX_OBJS): %.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

all: $(TARGET)
#cp -f $(TARGET) $(LIB_DIR)

$(TARGET): $(CC_OBJS) $(CXX_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS) -o $@
	mkdir -p $(LIBMOD_DIR)
	-install -m 555 ${TARGET} $(LIBMOD_DIR)
clean:
	$(RM) $(TARGET_OBJS) $(TARGET)
