/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#ifndef __HLS_M3ULIVESESSION_H__
#define __HLS_M3ULIVESESSION_H__

/******************************************************************************

                  版权所有 (C), amlogic

 ******************************************************************************
  文 件 名   : hls_m3ulivesession.h
  版 本 号   : 初稿
  作    者   : peter
  生成日期   : 2013年2月21日 星期四
  最近修改   :
  功能描述   : hls_m3ulivesession.c 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年2月21日 星期四
    作    者   : peter
    修改内容   : 创建文件

******************************************************************************/

/*----------------------------------------------*
 * 包含头文件                                   *
 *----------------------------------------------*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "libavformat/avio.h"
#include "libavformat/avformat.h"
#include <amthreadpool.h>
#include "hls_m3uparser.h"
#include "hls_utils.h"

/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/

#define AES_BLOCK_SIZE 16
#define HASH_KEY_SIZE   16
#define FAILED_RETRIES_MAX 5
#define PLAYBACK_SESSION_ID_MAX 128
#define USE_SIMPLE_CACHE 1
#define BW_MEASURE_ITEM_DEFAULT 100
#define DOWNLOAD_EXIT_CODE 0x1fffffff
#define MEDIA_TYPE_NUM 3

#define AUDIO_BANDWIDTH_MAX 150000  //150k
#define BANDWIDTH_THRESHOLD 5000 //5k
#define POS_SEEK_THRESHOLD 5000000 //5s
#define MEDIA_CACHED_BUFFER_THREASHOLD 10 // 10s

typedef struct _AESKeyForUrl {
    char keyUrl[MAX_URL_SIZE];/* url key path */
    uint8_t keyData[AES_BLOCK_SIZE]; /* AES-128 */
    struct list_head key_head;
} AESKeyForUrl_t;

typedef struct _SessionMediaItem {
    int64_t media_monitor_timer;
    int64_t media_last_fetch_timeUs;
    int64_t media_seek_timeUs;
    int64_t media_switch_anchor_timeUs; // select/unselect track.
    uint8_t media_last_bandwidth_list_hash[HASH_KEY_SIZE];
    MediaType media_type;
    char * media_url;
    char * media_redirect;
    char * media_last_m3u8_url;
    char * media_last_segment_url;
    char * media_cookies;
    void * media_playlist;
    void * media_cache;
    void * session;
    int media_cur_seq_num;
    int media_first_seq_num;
    int media_cur_bandwidth_index;
    int media_estimate_bandwidth_bps;
    int media_estimate_bps;
    int media_refresh_state;
    int media_retries_num;
    int media_err_code;
    int media_eof_flag;
    int media_seek_flag;
    int media_handling_seek;
    int media_no_new_file;
    int media_codec_buffer_time_s; // just an approximate value.
    int media_sub_ready;
    int media_encrypted;
    int media_aes_keyurl_list_num;
    FILE * media_dump_handle;
    struct list_head media_aes_key_list;
    pthread_t media_tid;
    pthread_mutex_t media_lock;
    pthread_cond_t media_cond;
} SessionMediaItem;

typedef struct _BandwidthItem {
    int index;
    char* url;
    char* redirect;
    unsigned long mBandwidth;
    int program_id;
    void* playlist;
    M3uBaseNode * node;
    M3uKeyInfo * baseScriptkeyinfo;
} BandwidthItem_t;

typedef struct _M3ULiveSession {
    char* baseUrl;
    char* last_m3u8_url;
    char* redirectUrl;
    char* headers;
    char* last_timeshift_segment_url;
    char* last_segment_url;
    char* stbId_string;
    void* master_playlist;
    void* playlist;
    guid_t session_guid;
    int is_opened;
    int is_variant;
    int is_mediagroup;
    int is_except;
    int is_livemode;
    int timeshift_force_refresh;
    int need_refresh_playlist; // <=0: normal live mode; 1: timeshift
    int timeshift_url_switch;
    BandwidthItem_t** bandwidth_list;
    AESKeyForUrl_t** aes_keyurl_list;
    SessionMediaItem * media_item_array[MEDIA_TYPE_NUM];
    int media_item_num;
    int media_dump_mode;
    int aes_keyurl_list_num;
    int start_min_bw;
    int bandwidth_item_num;
    int prev_bandwidth_index;
    int refresh_state;
    int cur_seq_num;
    int first_seq_num;
    int retries_num;
    int is_closed;
    int seekflag;
    int handling_seek;
    int log_level;
    int codec_data_time;
    int estimate_bandwidth_bps;
    int64_t cached_data_timeUs;
    int is_ts_media;
    int is_encrypt_media;
    int is_http_ignore_range;
    int target_duration;
    int stream_estimate_bps;/* Try to estimate the bandwidth for this stream */
    int64_t seektimeUs;
    int64_t seekposByte;
    int64_t refresh_point;
    pthread_t tid;
    int64_t durationUs;
    int64_t last_bandwidth_list_fetch_timeUs;
    int64_t download_monitor_timer;
    uint8_t last_bandwidth_list_hash[HASH_KEY_SIZE];
    void* cache;
    void* bw_meausure_handle;
    time_t last_timepoint;
    time_t last_timepoint_end;
    int err_code;
    int eof_flag;
    int force_refresh;
    pthread_mutex_t session_lock;
    pthread_cond_t  session_cond;
    int (*interrupt)(void);
    int64_t output_stream_offset;
    int startsegment_index;
    void *urlcontext;
    int *last_notify_err_seq_num;
    int no_new_file_can_download;
    char *ext_gd_seek_info;
    char *cookies;
} M3ULiveSession;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


    /*----------------------------------------------*
     * 外部变量说明                                 *
     *----------------------------------------------*/

    /*----------------------------------------------*
     * 外部函数原型说明                             *
     *----------------------------------------------*/


    int m3u_session_open(const char* baseUrl, const char* headers, void** hSession, void *urlcontext);
    int m3u_session_is_seekable(void* hSession);
    int64_t m3u_session_seekUs(void* hSession, int64_t posUs, int (*interupt_func_cb)());
    int64_t m3u_session_seekUs_offset(void* hSession, int64_t posUs, int64_t *streamoffset);
    int m3u_session_get_durationUs(void*session, int64_t* dur);

    int m3u_session_get_cached_data_time(void*session, int* time);

    int m3u_session_get_estimate_bandwidth(void*session, int* bps);

    int m3u_session_get_error_code(void*session, int* errcode);

    int m3u_session_get_stream_num(void* session, int* num);
    int m3u_session_get_cur_bandwidth(void* session, int* bw);

    int m3u_session_set_codec_data(void* session, int time);

    int m3u_session_read_data(void* session, void* buf, int len);

    int m3u_session_close(void* hSession);

    int m3u_session_register_interrupt(void* session, int (*interupt_func_cb)());

    //ugly codes for cmf
    int64_t m3u_session_get_next_segment_st(void* session);
    int m3u_session_get_segment_num(void* session);
    int64_t m3u_session_hybrid_seek(void* session, int64_t seg_st, int64_t pos, int (*interupt_func_cb)());
    void* m3u_session_seek_by_index(void* session, int prev_index, int index, int (*interupt_func_cb)());
    int64_t m3u_session_get_segment_size(void* session, const char* url, int index, int type);
    void* m3u_session_get_index_by_timeUs(void* session, int64_t timeUs);
    void* m3u_session_get_segment_info_by_index(void* hSession, int index);

    // api for media group
    int m3u_session_media_read_data(void * session, int stream_index, uint8_t * buf, int len);
    int m3u_session_media_peek_cache_data(void * session, int stream_index);
    int m3u_session_media_get_current_bandwidth(void * session, int stream_index, int * bw);
    int m3u_session_media_set_codec_buffer_time(void * session, int stream_index, int buffer_time_s);
    int m3u_session_media_get_track_count(void * session);
    M3uTrackInfo * m3u_session_media_get_track_info(void * session, int index);
    int m3u_session_media_select_track(void * session, int index, int select, int64_t anchorTimeUs);
    int m3u_session_media_get_selected_track(void * session, MediaTrackType type);
    M3uSubtitleData * m3u_session_media_read_subtitle(void * session, int index);
    MediaType m3u_session_media_get_type_by_index(void * session, int index);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HLS_M3ULIVESESSION_H__ */
