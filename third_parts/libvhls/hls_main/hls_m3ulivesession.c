/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


//coded by peter,20130221

//#define LOG_NDEBUG 0
#define LOG_TAG "M3uSession"

#include "hls_m3ulivesession.h"
#include "hls_download.h"
#include "hls_bandwidth_measure.h"

#ifdef HAVE_ANDROID_OS
#include "hls_common.h"
#else
#include "hls_debug.h"
#endif

#if defined(HAVE_ANDROID_OS)
#include <openssl/md5.h>
#endif


#include <dlfcn.h>


typedef int VERIMATRIXgetkeyFunc(char* keyurl, uint8_t* keydat);
static VERIMATRIXgetkeyFunc* VERIMATRIXgetkey = NULL;
static VERIMATRIXgetkeyFunc* verimatrix_get_key()
{
    void * mLibHandle = dlopen("libVCASCommunication.so", RTLD_NOW);

    if (mLibHandle == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Unable to locate libVCASCommunication.so\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_ERROR, "verimatrix_get_key\n");

    return (VERIMATRIXgetkeyFunc*)dlsym(mLibHandle, "getvrkey");
}
/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/
static int64_t get_clock_monotonic_us(void);
/*----------------------------------------------*
 * 全局变量                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/

#ifdef USE_SIMPLE_CACHE
#include "hls_simple_cache.h"
#endif

// for timeshift of chinamobile
#define TIMESHIFT_URL_STARTTIME "%Y%m%dT%H%M%S.00Z"
#define TIMESHIFT_URL_TAG  "%s&starttime=%s&token="

const char * EXCEPSOURCE = "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8";

#define ERROR_MSG() LOGE("Null session pointer check:%s,%s,%d\n",__FILE__,__FUNCTION__,__LINE__)
enum RefreshState {
    INITIAL_MINIMUM_RELOAD_DELAY,
    FIRST_UNCHANGED_RELOAD_ATTEMPT,
    SECOND_UNCHANGED_RELOAD_ATTEMPT,
    THIRD_UNCHANGED_RELOAD_ATTEMPT
};

static void * _media_download_worker(void * ctx);
static SessionMediaItem * _init_session_mediaItem(M3ULiveSession * ss, MediaType type, const char * groupID, const char * url);
static int _reinit_session_mediaItem(M3ULiveSession * ss, SessionMediaItem * mediaItem, int bandwidth_index);

//================================misc=====================================================
static int64_t get_clock_monotonic_us(void)
{
    struct timespec new_time;
    int64_t cpu_clock_us = 0;

    clock_gettime(CLOCK_MONOTONIC, &new_time);
    cpu_clock_us = ((int64_t)new_time.tv_nsec / 1000 + (int64_t)new_time.tv_sec * 1000000);
    return cpu_clock_us;
}
static void _init_m3u_live_session_context(M3ULiveSession* ss)
{
    memset(ss, 0, sizeof(M3ULiveSession));
    ss->start_min_bw = -1;
    ss->prev_bandwidth_index = -1;
    ss->seekflag = -1;
    ss->handling_seek = -1;
    ss->seektimeUs = -1;
    ss->durationUs = -2;
    ss->cur_seq_num = -1;
    ss->first_seq_num = -1;
    ss->last_bandwidth_list_fetch_timeUs = -1;
    ss->seekposByte = -1;
    ss->is_ts_media = -1;
    ss->is_mediagroup = -1;
    ss->is_variant = -1;
    ss->is_except = -1;
    ss->is_livemode = -1;
    ss->timeshift_force_refresh = -1;
    ss->need_refresh_playlist = -1;
    ss->timeshift_url_switch = -1;
    ss->refresh_point = -1;
    ss->last_timepoint = -1;
    ss->last_timepoint_end  = -1;
    ss->is_encrypt_media = -1;
    ss->codec_data_time = -1;
    ss->master_playlist = NULL;
    ss->redirectUrl = NULL;
    ss->last_m3u8_url = NULL;
    ss->cache = NULL;
    ss->last_timeshift_segment_url = NULL;
    ss->last_segment_url = NULL;
    ss->refresh_state = INITIAL_MINIMUM_RELOAD_DELAY;
    ss->log_level = 0;
    ss->force_refresh = -1;
    ss->interrupt = NULL;
    if (in_get_sys_prop_bool("media.amplayer.disp_url") != 0) {
        ss->log_level = HLS_SHOW_URL;
    }
    float db = in_get_sys_prop_float("libplayer.hls.debug");
    if (db > 0) {
        ss->log_level = db;
    }
    if (in_get_sys_prop_float("libplayer.hls.ignore_range") > 0) {
        ss->is_http_ignore_range = 1;
    }
    ss->media_dump_mode = in_get_sys_prop_float("libplayer.hls.media_dump");
    int i;
    for (i = 0; i < MEDIA_TYPE_NUM; i++) {
        ss->media_item_array[i] = NULL;
    }
    ss->media_item_num = 0;
    //init session id
    in_generate_guid(&ss->session_guid);
    pthread_mutex_init(&ss->session_lock, NULL);
    pthread_cond_init(&ss->session_cond, NULL);

    ss->output_stream_offset = 0;
    ss->startsegment_index = 0;
    ss->urlcontext = NULL;
    ss->last_notify_err_seq_num = -1;
    ss->cookies = NULL;
}




static void _sort_m3u_session_bandwidth(M3ULiveSession* ss)
{
    if (ss == NULL) {
        ERROR_MSG();
        return;
    }

    int i = ss->bandwidth_item_num, j;
    if (i <= 1) {
        LOGV("Only one item,never need bubble sort\n");
        return;
    }
    BandwidthItem_t* temp = NULL;
    while (i > 0) {
        for (j = 0; j < i - 1; j++) {
            if (ss->bandwidth_list[j]->mBandwidth > ss->bandwidth_list[j + 1]->mBandwidth) {
                temp = ss->bandwidth_list[j];
                ss->bandwidth_list[j] = ss->bandwidth_list[j + 1];
                ss->bandwidth_list[j + 1] = temp;
            }
        }
        i--;
    }

    /* m3u8 bandwidth may not be compatible with HLS draft, fix it*/
    int coeff = 1;
    if (ss->bandwidth_list[ss->bandwidth_item_num - 1]->mBandwidth > 0
        && ss->bandwidth_list[ss->bandwidth_item_num - 1]->mBandwidth < BANDWIDTH_THRESHOLD) {
        coeff = 1000;
    }
    LOGI("*************************Dump all bandwidth list start ********************\n");
    for (i = 0; i < ss->bandwidth_item_num; i++) {
        if (ss->bandwidth_list[i]) {
            ss->bandwidth_list[i]->index = i;
            temp = ss->bandwidth_list[i];
            temp->mBandwidth *= coeff;
            if (ss->log_level >= HLS_SHOW_URL) {
                LOGI("***Item index:%d,Bandwidth:%lu,url:%s\n", temp->index, temp->mBandwidth, temp->url);
            } else {
                LOGI("***Item index:%d,Bandwidth:%lu\n", temp->index, temp->mBandwidth);
            }

        }
    }
    LOGI("*************************Dump all bandwidth list  end ********************\n");
}

#define ADD_TSHEAD_RECALC_DISPTS_TAG    ("amlogictsdiscontinue")
static const uint8_t ts_segment_lead[188] = {0x47, 0x00, 0x1F, 0xFF, 0,};

//just a strange function for sepcific purpose,sample size must greater than 3
static int _ts_simple_analyze(const uint8_t *buf, int size)
{
    int i;
    int isTs = 0;

    for (i = 0; i < size - 3; i++) {
        if (buf[i] == 0x47 && !(buf[i + 1] & 0x80) && (buf[i + 3] != 0x47)) {
            isTs = 1;
            break;
        }
    }
    LOGV("ts_simple_analyze isTs is [%d]\n", isTs);

    return isTs;
}
static void _generate_fake_ts_leader_block(uint8_t* buf, int size, int segment_durMs)
{
    if (buf == NULL || size < 188) {
        ERROR_MSG();
        return;
    }
    int taglen = strlen(ADD_TSHEAD_RECALC_DISPTS_TAG);
    memcpy(buf, ts_segment_lead , 188);
    memcpy(buf + 4, &segment_durMs, 4);
    memcpy(buf + 8, ADD_TSHEAD_RECALC_DISPTS_TAG, taglen);

    return;
}

static int _is_add_fake_leader_block(M3ULiveSession* ss)
{
    if (ss == NULL) {
        ERROR_MSG();
        return -1;
    }
    if (ss->durationUs > 0) { //just for live
        return 0;
    }
    if (ss->is_ts_media > 0 && in_get_sys_prop_float("libplayer.netts.recalcpts") > 0) {
        LOGV("Live streaming,soft demux,open add fake ts leader\n");
        return 1;
    }
    return 0;
}
static void* _fetch_play_list(const char* url, M3ULiveSession* ss, SessionMediaItem * mediaItem, int* unchanged, int bw_index)
{
    *unchanged = 0;
    void * buf = NULL;
    int blen = -1;
    int ret = -1;
    char * redirectUrl = NULL;
    char * cookies = NULL;
    char * last_m3u8_url = NULL;
    char headers[MAX_URL_SIZE] = {0};

    if (mediaItem) {
        cookies = mediaItem->media_cookies;
        last_m3u8_url = mediaItem->media_last_m3u8_url;
    } else {
        cookies = ss->cookies;
        last_m3u8_url = ss->last_m3u8_url;
    }

    snprintf(headers, MAX_URL_SIZE,
             "X-Playback-Session-Id: "GUID_FMT"", GUID_PRINT(ss->session_guid));

    if (ss->headers != NULL && strlen(ss->headers) > 0) {
        snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\n%s", ss->headers);
    } else {
        if (in_get_sys_prop_bool("media.libplayer.curlenable") <= 0 || !strstr(url, "https://")) {
            snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\n");
        }
    }
    if (cookies) {
        if (strlen(cookies) > 0) {
            if (ss->headers != NULL && strlen(ss->headers) > 0 && ss->headers[strlen(ss->headers) - 1] != '\n') {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", cookies);
            } else {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", cookies);
            }
        }
    }
    if (mediaItem) {
        ret = fetchHttpSmallFile(url, headers, &buf, &blen, &redirectUrl, &mediaItem->media_cookies);
    } else {
        ret = fetchHttpSmallFile(url, headers, &buf, &blen, &redirectUrl, &ss->cookies);
    }
    if (ret != 0) {
        if (buf != NULL) {
            free(buf);
        }
        if (mediaItem) {
            if (ret != HLSERROR(EINTR) && ret != HLSERROR(EIO)) {
                if (mediaItem->media_redirect) {
                    free(mediaItem->media_redirect);
                    mediaItem->media_redirect = NULL;
                }
            }
            mediaItem->media_err_code = -ret;
            if ((ret == HLSERROR(EINTR) || ret == HLSERROR(EIO)) && (ss->seekflag > 0 || mediaItem->media_seek_flag == 2)) {
                if (mediaItem->media_seek_flag != 2) {
                    mediaItem->media_seek_flag = 0;
                }
            }
        } else {
            if (ret != HLSERROR(EINTR) && ret != HLSERROR(EIO)) {
                if (ss->bandwidth_item_num <= 0) {
                    if (ss->redirectUrl) {
                        free(ss->redirectUrl);
                        ss->redirectUrl = NULL;
                    }
                } else {
                    if (bw_index >= 0 && ss->bandwidth_list[bw_index]->redirect) {
                        free(ss->bandwidth_list[bw_index]->redirect);
                        ss->bandwidth_list[bw_index]->redirect = NULL;
                    }
                }
            }
            ss->err_code = -ret;//small trick,avoid to exit player thread
            if ((ret == HLSERROR(EINTR) || ret == HLSERROR(EIO)) && ss->seekflag > 0) {
                ss->seekflag = 0;
            }
        }
        return NULL;
    }

    if (mediaItem) {
        if (redirectUrl) {
            LOGV("[Type : %d] Got base re-direct url,location:%s", mediaItem->media_type, redirectUrl);
            if (mediaItem->media_redirect) {
                free(mediaItem->media_redirect);
            }
            mediaItem->media_redirect = redirectUrl;
        }
    } else {
        if (ss->bandwidth_item_num <= 0) {
            if (redirectUrl) {
                LOGV("Got base re-direct url,location:%s\n", redirectUrl);
                if (ss->redirectUrl) {
                    free(ss->redirectUrl);
                }
                ss->redirectUrl = redirectUrl;
            }
        } else {
            if (redirectUrl && bw_index >= 0) {
                LOGV("Got re-direct url,location:%s, bandwidth:%d\n", redirectUrl, bw_index);
                if (ss->bandwidth_list[bw_index]->redirect) {
                    free(ss->bandwidth_list[bw_index]->redirect);
                }
                ss->bandwidth_list[bw_index]->redirect = redirectUrl;
            }
        }
    }

    // MD5 functionality is not available on the simulator, treat all
    // bandwidth_lists as changed. from android codes

#if defined(HAVE_ANDROID_OS)
    if (last_m3u8_url && strcmp(last_m3u8_url, url)) {
        goto PASS_THROUGH;
    }

    uint8_t hash[16];

    MD5_CTX m;
    MD5_Init(&m);
    MD5_Update(&m, buf, blen);

    MD5_Final(hash, &m);

    if (mediaItem) {
        if (mediaItem->media_playlist && !memcmp(hash, mediaItem->media_last_bandwidth_list_hash, HASH_KEY_SIZE)) {
            if (mediaItem->media_refresh_state != THIRD_UNCHANGED_RELOAD_ATTEMPT) {
                mediaItem->media_refresh_state = (enum RefreshState)(mediaItem->media_refresh_state + 1);
            }
            *unchanged = 1;
            LOGI("[Type : %d] Playlist unchanged, refresh state is now %d", mediaItem->media_type, (int)mediaItem->media_refresh_state);
            free(buf);
            return NULL;
        }
        memcpy(mediaItem->media_last_bandwidth_list_hash, hash, sizeof(hash));
    } else {
        if (ss->playlist != NULL && !memcmp(hash, ss->last_bandwidth_list_hash, HASH_KEY_SIZE)) {
            // bandwidth_list unchanged

            if (ss->refresh_state != THIRD_UNCHANGED_RELOAD_ATTEMPT) {
                ss->refresh_state = (enum RefreshState)(ss->refresh_state + 1);
            }

            *unchanged = 1;

            LOGI("Playlist unchanged, refresh state is now %d",
                 (int)ss->refresh_state);
            free(buf);
            return NULL;
        }

        memcpy(ss->last_bandwidth_list_hash, hash, sizeof(hash));
    }

PASS_THROUGH:
    if (mediaItem) {
        mediaItem->media_refresh_state = INITIAL_MINIMUM_RELOAD_DELAY;
    } else {
        ss->refresh_state = INITIAL_MINIMUM_RELOAD_DELAY;
    }
#endif

    if (last_m3u8_url) {
        free(last_m3u8_url);
    }
    if (mediaItem) {
        mediaItem->media_last_m3u8_url = strdup(url);
    } else {
        ss->last_m3u8_url = strdup(url);
    }
    void * bandwidth_list = NULL;
    if (!redirectUrl) {
        ret = m3u_parse(url, buf, blen, &bandwidth_list);
    } else {
        ret = m3u_parse(redirectUrl, buf, blen, &bandwidth_list);
    }
    if (ret != 0 || m3u_is_extm3u(bandwidth_list) <= 0) {
        LOGE("failed to parse .m3u8 bandwidth_list");
        free(buf);
        return NULL;
    }
    if (buf) {
        free(buf);
    }
    return bandwidth_list;

}
static int  _time_to_refresh_bandwidth_list(M3ULiveSession* ss, SessionMediaItem * item, int64_t nowUs)
{
    int64_t last_fetch_timeUs = -1;
    int refresh_state = INITIAL_MINIMUM_RELOAD_DELAY;
    void * playlist = NULL;
    if (item) {
        playlist = item->media_playlist;
        refresh_state = item->media_refresh_state;
        last_fetch_timeUs = item->media_last_fetch_timeUs;
    } else {
        playlist = ss->playlist;
        refresh_state = ss->refresh_state;
        last_fetch_timeUs = ss->last_bandwidth_list_fetch_timeUs;
    }

    if (playlist == NULL) {
        if (refresh_state == INITIAL_MINIMUM_RELOAD_DELAY) {
            return 1;
        }
    }

    int32_t targetDurationSecs = 10;

    if (playlist != NULL) {
        targetDurationSecs = m3u_get_target_duration(playlist);
    }

    int64_t targetDurationUs = targetDurationSecs * 1000000ll;

    int64_t minPlaylistAgeUs = 0;

    switch (refresh_state) {
    case INITIAL_MINIMUM_RELOAD_DELAY: {
        size_t n = m3u_get_node_num(playlist);
        if (n > 0) {
            M3uBaseNode* node = m3u_get_node_by_index(playlist, n - 1);
            if (node) {
                minPlaylistAgeUs = node->durationUs;
                break;
            }

        }
        minPlaylistAgeUs = targetDurationUs;
        // fall through
    }

    case FIRST_UNCHANGED_RELOAD_ATTEMPT: {
        minPlaylistAgeUs = targetDurationUs / 2;
        break;
    }

    case SECOND_UNCHANGED_RELOAD_ATTEMPT: {
        minPlaylistAgeUs = (targetDurationUs * 3) / 2;
        break;
    }

    case THIRD_UNCHANGED_RELOAD_ATTEMPT: {
        minPlaylistAgeUs = targetDurationUs * 3;
        break;
    }

    default:
        LOGV("Never see this line\n");
        break;
    }

    int flag = 0;
    if (last_fetch_timeUs + minPlaylistAgeUs <= nowUs) {
        if (item) {
            LOGV("[Type : %d] Reach to the time to refresh list", item->media_type);
        } else {
            LOGV("Reach to the time to refresh list");
        }
        flag = 1;
    } else {
        if (item) {
            LOGV("[Type : %d] Last fetch list timeUs:%lld,min playlist agesUs:%lld,nowUs:%lld",
                item->media_type, last_fetch_timeUs, minPlaylistAgeUs, nowUs);
        } else {
            LOGV("Last fetch list timeUs:%lld,min playlist agesUs:%lld,nowUs:%lld",
                last_fetch_timeUs, minPlaylistAgeUs, nowUs);
        }
    }
    return flag;
}

static int _estimate_and_calc_bandwidth(M3ULiveSession* s)
{
    if (s == NULL) {
        ERROR_MSG();
        return 0;
    }
    int fast_bw, mid_bw, avg_bw, calc_bw;
    bandwidth_measure_get_bandwidth(s->bw_meausure_handle, &fast_bw, &mid_bw, &avg_bw);
    calc_bw = fast_bw * 0.8 + mid_bw * 0.2;
    LOGV("Get current bw.fast:%.2f kbps,mid:%.2f kbps,avg:%.2f kbps,calc value:%.2f kbps\n",
         fast_bw / 1024.0f, mid_bw / 1024.0f, avg_bw / 1024.0f, calc_bw / 1024.0f);
    return calc_bw;
}
#define CODEC_BUFFER_LOW_FLAG  (8)          // 8s
#define CODEC_BUFFER_HIGH_FLAG (15)          //15s
static int  _get_best_bandwidth_index(M3ULiveSession* s, SessionMediaItem * mediaItem) //rate adaptation logic
{
    int index = 0, prev_bandwidth_index = 0, tmp_cur_seq_num = 0;
    void * playlist = NULL;
    if (mediaItem) {
        prev_bandwidth_index = mediaItem->media_cur_bandwidth_index;
        playlist = mediaItem->media_playlist;
        tmp_cur_seq_num = mediaItem->media_cur_seq_num;
    } else {
        prev_bandwidth_index = s->prev_bandwidth_index;
        playlist = s->playlist;
        tmp_cur_seq_num = s->cur_seq_num;
    }
    int adaptive_profile = in_get_sys_prop_float("libplayer.hls.profile");
    if (adaptive_profile == 0) {
        int fixed_bw = in_get_sys_prop_float("libplayer.hls.fixed_bw");
        if (fixed_bw >= 0 && fixed_bw <= (s->bandwidth_item_num - 1) && fixed_bw != prev_bandwidth_index) {
            index = fixed_bw;
        } else {
            index = prev_bandwidth_index;
        }
        return index;
    }
    if (s->bandwidth_item_num > 0 && adaptive_profile != 0 && s->seekflag <= 0 && s->is_closed <= 0) {
        int reserved_segment_check = 0;
        if (playlist != NULL && m3u_is_complete(playlist) > 0) {
            int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(playlist, 0)->media_sequence;
            if (firstSeqNumberInPlaylist == -1) {
                firstSeqNumberInPlaylist = 0;
            }
            reserved_segment_check = m3u_get_node_num(playlist) - (tmp_cur_seq_num - firstSeqNumberInPlaylist);
            if (mediaItem) {
                LOGV("[Type : %d] Reserved %d segment in playlist for download.\n", mediaItem->media_type, reserved_segment_check);
            } else {
                LOGV("Reserved %d segment in playlist for download.\n", reserved_segment_check);
            }
            if (reserved_segment_check < 3) { //last two item,will never change bandwidth
                index = prev_bandwidth_index;
                return index;
            }
        }
        int est_bps;
        if (mediaItem) {
            est_bps = mediaItem->media_estimate_bandwidth_bps;
        } else {
            est_bps = s->estimate_bandwidth_bps;
        }
        if (mediaItem) {
            LOGI("[Type : %d] bandwidth estimated at %.2f kbps", mediaItem->media_type, est_bps / 1024.0f);
        } else {
            LOGI("bandwidth estimated at %.2f kbps", est_bps / 1024.0f);
        }
        if (est_bps == 0) {
            if (mediaItem) {
                LOGV("[Type : %d] no bandwidth estimate.Pick the lowest bandwidth stream by default.", mediaItem->media_type);
            } else {
                LOGV("no bandwidth estimate.Pick the lowest bandwidth stream by default.");
            }
            return prev_bandwidth_index;
        }

        long maxBw = (long)in_get_sys_prop_float("media.httplive.max-bw");
        if (maxBw > 0 && est_bps > maxBw) {
            LOGV("bandwidth capped to %ld bps", maxBw);
            est_bps = maxBw;
        }
        // Consider only 80% of the available bandwidth usable.
        est_bps = (est_bps * 8) / 10;

        index = s->bandwidth_item_num - 1;
        while (index > 0 && (s->bandwidth_list[index]->mBandwidth > (size_t)est_bps)) {
            --index;
        }
    } else {
        if (s->seekflag > 0 && in_get_sys_prop_bool("libplayer.hls.lowbw_seek") > 0) {
            LOGV("Used low bandwidth stream after seek");
            index = 0;
        } else {
            index = prev_bandwidth_index;
        }
    }
    return index;
}

#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16
#endif

static int _get_decrypt_key(M3ULiveSession* s, SessionMediaItem * mediaItem, int playlistIndex, AESKeyInfo_t* key)
{
    int found = 0;
    char* method;
    M3uBaseNode* node = NULL;
    ssize_t i ;
    int is_encrypt_media = -1, aes_keyurl_list_num = 0;
    void * playlist = NULL;
    char * cookies = NULL;
    AESKeyForUrl_t * aes_keyurl_list = NULL;
    if (mediaItem) {
        is_encrypt_media = mediaItem->media_encrypted;
        aes_keyurl_list_num = mediaItem->media_aes_keyurl_list_num;
        playlist = mediaItem->media_playlist;
        cookies = mediaItem->media_cookies;
    } else {
        is_encrypt_media = s->is_encrypt_media;
        aes_keyurl_list_num = s->aes_keyurl_list_num;
        playlist = s->playlist;
        cookies = s->cookies;
    }
    if (is_encrypt_media == 0) {
        return -1;
    }
    for (i = playlistIndex; i >= 0; --i) {
        node = m3u_get_node_by_index(playlist, i);
        if (node != NULL && node->flags & CIPHER_INFO_FLAG) {
            method = node->key->method;
            found = 1;
            break;
        }
        if (s->is_closed) {
            LOGV("Got close flag\n");
            return -1;
        }
    }
    if (!found) {
        method = "NONE";
    }

    if (!strcmp(method, "NONE")) {
        return 0;
    } else if (strcmp(method, "AES-128") ) { //"PRAESCTR" for PlayReady DRM
        LOGE("Unsupported cipher method '%s'", method);
        return -1;
    }
    const char* keyUrl = node->key->keyUrl;
    int index = -1;
    if (aes_keyurl_list_num > 0) {
        if (mediaItem) {
            AESKeyForUrl_t * pos = NULL;
            AESKeyForUrl_t * tmp = NULL;
            int count = 0;
            list_for_each_entry_safe(pos, tmp, &mediaItem->media_aes_key_list, key_head) {
                if (!strncmp(keyUrl, pos->keyUrl, MAX_URL_SIZE)) {
                    index = count;
                    aes_keyurl_list = pos;
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGI("[Type : %d] Found aes key,url:%s,index:%d\n", mediaItem->media_type, keyUrl, index);
                    } else {
                        LOGI("Found aes key,index:%d\n", index);
                    }
                    break;
                }
                if (s->is_closed) {
                    LOGV("Got close flag\n");
                    return -1;
                }
                count++;
            }
        } else {
            for (i = 0; i < aes_keyurl_list_num; i++) {
                if (!strncmp(keyUrl, s->aes_keyurl_list[i]->keyUrl, MAX_URL_SIZE)) {
                    index = i;
                    aes_keyurl_list = s->aes_keyurl_list[i];
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGI("Found aes key,url:%s,index:%d\n", keyUrl, index);
                    } else {
                        LOGI("Found aes key,index:%d\n", index);
                    }
                    break;
                }
                if (s->is_closed) {
                    LOGV("Got close flag\n");
                    return -1;
                }
            }
        }
    }

    uint8_t* keydat = NULL;
    if (index >= 0) {
        keydat = aes_keyurl_list->keyData;
        LOGV("Got cached key.");
    } else { //
        if (s->urlcontext && ((URLContext *)(s->urlcontext))->prot && !strcasecmp(((URLContext *)(s->urlcontext))->prot->name, "vrwc")) {
#ifndef ENABLE_VIEWRIGHT_WEB
            LOGE("Verimatrix link need enable BUILD_WITH_VIEWRIGHT_WEB and place your own  libViewRightWebClient.so!");
            return -1;
#else
            LOGV("Verimatrix link: try to get verimatrix aes key\n");

            if (VERIMATRIXgetkey == NULL) {
                VERIMATRIXgetkey =  verimatrix_get_key();
                if (VERIMATRIXgetkey == NULL) {
                    LOGE("verimatrix_get_key dlsym fail\n");
                    return -1;
                }
            }
            keydat = malloc(AES_BLOCK_SIZE);
            if (keydat) {
                int ret = VERIMATRIXgetkey(keyUrl, keydat);
                if (ret) {
                    free(keydat);
                    LOGE("Failed to get verimatrix aes key\n");
                    return -1;
                }
            } else {
                return -1;
            }
#endif
        }  else {

            int isize = 0;
            int ret = -1;
            char* redirectUrl = NULL;
            char headers[MAX_URL_SIZE] = {0};
            if (s->headers != NULL) {
                strncpy(headers, s->headers, MAX_URL_SIZE);
            }
            if (cookies && strlen(cookies) > 0) {
                if (s->headers != NULL && strlen(s->headers) > 0 && s->headers[strlen(s->headers) - 1] != '\n') {
                    snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", cookies);
                } else {
                    snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", cookies);
                }
            }
            if (mediaItem) {
                ret = fetchHttpSmallFile(keyUrl, headers, (void**)&keydat, &isize, &redirectUrl, &mediaItem->media_cookies);
            } else {
                ret = fetchHttpSmallFile(keyUrl, headers, (void**)&keydat, &isize, &redirectUrl, &s->cookies);
            }
            if (ret != 0) {
                if (mediaItem) {
                    LOGV("[Type : %d] Failed to get aes key!", mediaItem->media_type);
                } else {
                    LOGV("Failed to get aes key!");
                }
                return -1;
            }
            if (redirectUrl) {
                if (s->log_level >= HLS_SHOW_URL) {
                    LOGV("Display redirect url:%s\n", redirectUrl);
                }
                free(redirectUrl);
            }
        }


        AESKeyForUrl_t* anode = (AESKeyForUrl_t*)malloc(sizeof(AESKeyForUrl_t));
        memset(anode, 0, sizeof(AESKeyForUrl_t));

        strlcpy(anode->keyUrl, keyUrl, MAX_URL_SIZE);
        memcpy(anode->keyData, keydat, AES_BLOCK_SIZE);

        free(keydat);
        keydat = anode->keyData;
        if (mediaItem) {
            INIT_LIST_HEAD(&anode->key_head);
            list_add(&anode->key_head, &mediaItem->media_aes_key_list);
            mediaItem->media_aes_keyurl_list_num++;
        } else {
            in_dynarray_add(&s->aes_keyurl_list, &s->aes_keyurl_list_num, anode);
        }
    }


    const char* iv = node->key->iv;
    int is_iv_load = 0;
    if (iv != NULL && strlen(iv) > 0) {
        if (strncmp(iv, "0x", strlen("0x")) && strncmp(iv, "0X", strlen("0X"))) {
            LOGE("malformed cipher IV '%s'.\n", iv);
            return -1;
        }
        is_iv_load = 1;

    }
    unsigned char aes_ivec[AES_BLOCK_SIZE];
    memset(aes_ivec, 0, sizeof(aes_ivec));

    if (is_iv_load > 0) {
        for (i = 0; i < 16; ++i) {
            char c1 = tolower(iv[2 + 2 * i]);
            char c2 = tolower(iv[3 + 2 * i]);
            if (!isxdigit(c1) || !isxdigit(c2)) {
                LOGE("malformed cipher IV '%s'.", iv);
                return -1;
            }
            uint8_t nibble1 = isdigit(c1) ? c1 - '0' : c1 - 'a' + 10;
            uint8_t nibble2 = isdigit(c2) ? c2 - '0' : c2 - 'a' + 10;

            aes_ivec[i] = nibble1 << 4 | nibble2;
            if (s->is_closed) {
                LOGV("Got close flag\n");
                return -1;
            }
        }
    } else {
        aes_ivec[15] = playlistIndex & 0xff;
        aes_ivec[14] = (playlistIndex >> 8) & 0xff;
        aes_ivec[13] = (playlistIndex >> 16) & 0xff;
        aes_ivec[12] = (playlistIndex >> 24) & 0xff;

    }


    key->type = AES128_CBC;

    key->key_info = (AES128KeyInfo_t*)malloc(sizeof(AES128KeyInfo_t));
    if (key->key_info == NULL) {
        ERROR_MSG();
        return -1;
    }
    ((AES128KeyInfo_t*)key->key_info)->key_hex[32] = '\0';
    ((AES128KeyInfo_t*)key->key_info)->ivec_hex[32] = '\0';

    in_data_to_hex(((AES128KeyInfo_t*)key->key_info)->key_hex, keydat, AES_BLOCK_SIZE, 0);
    in_data_to_hex(((AES128KeyInfo_t*)key->key_info)->ivec_hex, aes_ivec, AES_BLOCK_SIZE, 0);
    in_hex_dump("AES key ", keydat, 16);
    in_hex_dump("AES IV ", aes_ivec, 16);
    return 0;
}

static char *get_datatime_str(const char* line)
{
    const char *start = strstr(line, "playseek=");
    const char *end = strstr(line, "-&zoneoffset");

    if ((start == NULL) || (end == NULL)) {
        LOGW("[%s:%d] start or end is NULL!\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    ssize_t colonPos = end - start;
    colonPos = colonPos - strlen("playseek=");
    LOGV("[%s:%d] colonPos: %d\n", __FUNCTION__, __LINE__, colonPos);
    char * dataTime = strdup(start + strlen("playseek="));
    if (dataTime == NULL) {
        LOGW("[%s:%d] dataTime malloc fail!\n", __FUNCTION__, __LINE__);
        return NULL;
    }
    dataTime[colonPos] = 0;

    int len = strlen(dataTime);
    LOGV("[%s:%d] dataTime: %s len: %d\n", __FUNCTION__, __LINE__, dataTime, len);
    int i = 0, j = 0;
    for (i = 0; i < len; i++) {
        if ((*(dataTime + i) < '0') || (*(dataTime + i) > '9')) {
            *(dataTime + i) = 0;
            for (j = i + 1; j < len; j++) {
                if ((*(dataTime + j) >= '0') && (*(dataTime + j) <= '9')) {
                    *(dataTime + i) = *(dataTime + j);
                    *(dataTime + j) = 0;
                    break;
                }
            }
        }
    }
    LOGV("[%s:%d] dataTime: %s\n", __FUNCTION__, __LINE__, dataTime);
    return dataTime;
}

static char *get_timeoffset_str(const char* line)
{
    const char *start = strstr(line, "timeoffset=");

    if (start == NULL) {
        LOGW("[%s:%d] start is NULL!\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    ssize_t colonPos = strlen(start) - strlen("timeoffset=");
    LOGV("[%s:%d] colonPos: %d\n", __FUNCTION__, __LINE__, colonPos);
    char * timeOffset = strdup(start + strlen("timeoffset="));
    if (timeOffset == NULL) {
        LOGW("[%s:%d] timeOffset malloc fail!\n", __FUNCTION__, __LINE__);
        return NULL;
    }
    timeOffset[colonPos] = 0;

    int len = strlen(timeOffset);
    LOGV("[%s:%d] timeOffset: %s len: %d\n", __FUNCTION__, __LINE__, timeOffset, len);
    int i = 0, j = 0;
    for (i = 0; i < len; i++) {
        if ((*(timeOffset + i) < '0') || (*(timeOffset + i) > '9')) {
            *(timeOffset + i) = 0;
            for (j = i + 1; j < len; j++) {
                if ((*(timeOffset + j) >= '0') && (*(timeOffset + j) <= '9')) {
                    *(timeOffset + i) = *(timeOffset + j);
                    *(timeOffset + j) = 0;
                    break;
                }
            }
        }
    }
    LOGV("[%s:%d] timeOffset: %s\n", __FUNCTION__, __LINE__, timeOffset);
    return timeOffset;
}

static char *get_ext_gd_seek_info(const char* str)
{
    int64_t datatime = 0, timeoffset = 0;
    char * dataTime = get_datatime_str(str);
    char * timeOffset = get_timeoffset_str(str);

    if ((dataTime != NULL) && (timeOffset != NULL)) {
        parseInt64(dataTime, &datatime);
        parseInt64(timeOffset, &timeoffset);
        LOGV("[%s:%d] datatime = %lld timeoffset = %lld\n", __FUNCTION__, __LINE__, datatime, timeoffset);
        free(dataTime);
        free(timeOffset);

        char * dataTime1 = malloc(32);
        if (dataTime1 == NULL) {
            LOGW("[%s:%d] dataTime1 malloc fail!\n", __FUNCTION__, __LINE__);
            return NULL;
        }
        if (timeoffset == 0) {
            sprintf(dataTime1, "%lld", datatime);
        } else {
            sprintf(dataTime1, "%lld", timeoffset);
        }
        LOGV("[%s:%d] dataTime1: %s\n", __FUNCTION__, __LINE__, dataTime1);

        const char *start = strstr(str, "-&zoneoffset");
        const char *end = strstr(str, "&timeoffset=");
        if ((start == NULL) || (end == NULL)) {
            LOGW("[%s:%d] start or end is NULL!\n", __FUNCTION__, __LINE__);
            free(dataTime1);
            return NULL;
        }
        ssize_t colonPos = end - start;
        LOGV("[%s:%d] colonPos: %d\n", __FUNCTION__, __LINE__, colonPos);
        char * ext_str = malloc(strlen(start) + strlen(dataTime1) + 1);
        if (ext_str == NULL) {
            LOGW("[%s:%d] ext_str malloc fail!\n", __FUNCTION__, __LINE__);
            free(dataTime1);
            return NULL;
        }
        strcpy(ext_str, dataTime1);
        strcat(ext_str, start);
        ext_str[colonPos + strlen(dataTime1)] = 0;
        LOGV("[%s:%d] ext_str: %s\n", __FUNCTION__, __LINE__, ext_str);
        free(dataTime1);
        return ext_str;
    }

    if (dataTime != NULL) {
        free(dataTime);
    }
    if (timeOffset != NULL) {
        free(timeOffset);
    }
    return NULL;
}

static int get_index_by_datatime(void* hParse, const char* line)
{
    int64_t datatime = 0;
    char * dataTime = get_datatime_str(line);

    if (dataTime == NULL) {
        LOGW("[%s:%d] dataTime is NULL!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    parseInt64(dataTime, &datatime);
    LOGV("[%s:%d] datatime = %lld\n", __FUNCTION__, __LINE__, datatime);
    M3uBaseNode* node = m3u_get_node_by_datatime(hParse, datatime);
    free(dataTime);
    int index = (node != NULL) ? node->index : -1;
    LOGV("[%s:%d] index = %d\n", __FUNCTION__, __LINE__, index);
    return index;
}

static int _get_valid_bandwidth_list(M3ULiveSession* s, int fail_index)
{
    BandwidthItem_t ** new_list = NULL;
    void * playlist = NULL;
    int i = 0;
    int unchanged = 0;
    int ret = -1;
    int number = 0;
    int index = 0;
    for (i = 0; i < s->bandwidth_item_num; i++) { // check each item's availability.
        if (i == fail_index) {
            continue;
        }
        playlist = _fetch_play_list(s->bandwidth_list[i]->url, s, NULL, &unchanged, i);
        if (playlist) {
            BandwidthItem_t* item = (BandwidthItem_t*)malloc(sizeof(BandwidthItem_t));
            item->url = strdup(s->bandwidth_list[i]->url);
            item->mBandwidth = s->bandwidth_list[i]->mBandwidth;
            item->program_id = s->bandwidth_list[i]->program_id;
            item->playlist = playlist;
            item->index = index++;
            in_dynarray_add(&new_list, &number, item);
        } else {
            if (s->log_level >= HLS_SHOW_URL) {
                LOGE("failed to load playlist at url '%s'", s->bandwidth_list[i]->url);
            }
        }
    }
    for (i = 0; i < s->bandwidth_item_num; i++) {
        BandwidthItem_t* item = s->bandwidth_list[i];
        if (item) {
            if (item->url != NULL) {
                free(item->url);
            }
            if (item->playlist != NULL) {
                m3u_release(item->playlist);
            }
            free(item);
        }
    }
    in_freepointer(&s->bandwidth_list);
    s->bandwidth_item_num = 0;
    s->playlist = NULL;
    if (number > 0) {
        s->bandwidth_list = new_list;
        s->bandwidth_item_num = number;
        s->prev_bandwidth_index = 0;
        s->playlist = s->bandwidth_list[0]->playlist;
        ret = 0;
        LOGI("Got new valid bandwidth list, num : %d \n", number);
    }
    return ret;
}

static void _set_session_para(M3ULiveSession * s, SessionMediaItem * item) {
    void * playlist = NULL;
    int32_t firstSeqNumberInPlaylist = 0, cur_seq_num_tmp = 0;
    int is_encrypt_media = -1;

    if (s->is_mediagroup <= 0) {
        playlist = s->playlist;
        is_encrypt_media = s->is_encrypt_media;
    } else {
        playlist = item->media_playlist;
        is_encrypt_media = item->media_encrypted;
    }

    M3uBaseNode* node = m3u_get_node_by_index(playlist, 0);
    firstSeqNumberInPlaylist = node->media_sequence;
    if (firstSeqNumberInPlaylist == -1) {
        firstSeqNumberInPlaylist = 0;
    }
    if (is_encrypt_media == -1) { //simply detect encrypted stream
        //add codes for stream that the field "METHOD" of EXT-X-KEY is "NONE".
        char* method = NULL;
        if (node->key != NULL && node->key->method != NULL) {
            method = node->key->method;
        } else {
            method = "NONE";
        }
        if (node->flags & CIPHER_INFO_FLAG && strcmp(method, "NONE")) {
            if (s->is_mediagroup <= 0) {
                s->is_encrypt_media = 1;
            } else {
                item->media_encrypted = 1;
            }
        } else {
            if (s->is_mediagroup <= 0) {
                s->is_encrypt_media = 0;
            } else {
                item->media_encrypted = 0;
            }
        }
    }
    int rv = -1;
    rv = in_get_sys_prop_float("libplayer.hls.stpos");
    int hasEnd = -1;
    hasEnd = m3u_is_complete(playlist);
    if (rv < 0) {
        if (s->ext_gd_seek_info != NULL) {
            int index = get_index_by_datatime(playlist, s->ext_gd_seek_info);
            s->durationUs = -1;
            if (index < 0) {
                cur_seq_num_tmp = index;
            } else {
                cur_seq_num_tmp = firstSeqNumberInPlaylist + index;
            }
        }

        if (!s->ext_gd_seek_info || (cur_seq_num_tmp < 0)) {
            if (hasEnd > 0) { //first item
                cur_seq_num_tmp = firstSeqNumberInPlaylist;
                if (s->durationUs == -2) {
                    s->durationUs = m3u_get_durationUs(playlist);
                }
            } else { //last third item
                if (m3u_get_node_num(playlist) > 3) {
                    cur_seq_num_tmp = firstSeqNumberInPlaylist + m3u_get_node_num(playlist) - 3;
                } else { //first item
                    cur_seq_num_tmp = firstSeqNumberInPlaylist;
                }
                s->durationUs = -1;
            }
        }
    } else {
        if (rv < m3u_get_node_num(playlist)) {
            cur_seq_num_tmp = firstSeqNumberInPlaylist + rv;
        }
        if (hasEnd > 0) {
            if (s->durationUs == -2) {
                s->durationUs = m3u_get_durationUs(playlist);
            }
        } else {
            s->durationUs = -1;
        }
    }
    s->target_duration = m3u_get_target_duration(playlist);
    s->last_bandwidth_list_fetch_timeUs = get_clock_monotonic_us();
    if (hasEnd == 0 && s->is_livemode == -1) {
        //cntv url without livemode=x, use TAG #ENDLIST in m3u8 playlist to identify live or vod
        s->is_livemode = 1;
    }
    if (s->log_level >= HLS_SHOW_URL) {
        LOGV("playback,first segment from seq:%d,url:%s\n", cur_seq_num_tmp, m3u_get_node_by_index(playlist, cur_seq_num_tmp - firstSeqNumberInPlaylist)->fileUrl);
    } else {
        LOGV("playback,first segment from seq:%d\n", cur_seq_num_tmp);
    }

    if (s->is_mediagroup <= 0) {
        s->cur_seq_num = cur_seq_num_tmp;
    } else {
        item->media_cur_seq_num = cur_seq_num_tmp;
        item->media_first_seq_num = firstSeqNumberInPlaylist;
        item->media_last_fetch_timeUs = get_clock_monotonic_us();
    }
}

static int _choose_bandwidth_and_init_playlist(M3ULiveSession* s)
{
    if (s == NULL) {
        LOGE("failed to init playlist\n");
        return -1;
    }
    s->prev_bandwidth_index = 0;
    if (s->playlist == NULL) {
        int bandwidthIndex = 0;
        int fixed_bw = in_get_sys_prop_float("libplayer.hls.fixed_bw");
        if (fixed_bw > 0) {
            if (fixed_bw > (s->bandwidth_item_num - 1)) {
                bandwidthIndex = s->bandwidth_item_num - 1;
            } else {
                bandwidthIndex = fixed_bw;
            }
        }
        if (!s->is_except) {
            bandwidthIndex = s->bandwidth_item_num - 1;
        }
        char* url = NULL;
        if (s->bandwidth_item_num > 0) {
            url = s->bandwidth_list[bandwidthIndex]->url;
            s->prev_bandwidth_index = bandwidthIndex;
        } else {
            LOGE("Never get bandwidth list\n");
            return -1;
        }

        int unchanged = 0, ret = 0;
        if (s->is_mediagroup <= 0) {
            void* playlist = _fetch_play_list(url, s, NULL, &unchanged, bandwidthIndex);
            if (playlist == NULL) {
                if (unchanged) {
                    LOGE("Never see this line\n");
                } else {
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, url);
                    }
                    ret = _get_valid_bandwidth_list(s, bandwidthIndex);
                    if (ret < 0) {
                        return ret;
                    }
                }
            } else {
                s->bandwidth_list[bandwidthIndex]->playlist = playlist;
                s->prev_bandwidth_index = bandwidthIndex;
                s->playlist = playlist;
            }
            if (m3u_get_node_num(s->playlist) == 0) {
                LOGE("Empty playlist,can't find one item\n");
                return -1;
            }
        } else {
            BandwidthItem_t * bandItem = s->bandwidth_list[bandwidthIndex];
            SessionMediaItem * mediaItem = NULL;
            uint32_t typeMask = m3u_get_media_type_by_codec(bandItem->node->codec);
            if (typeMask == TYPE_NONE) {
                typeMask |= (TYPE_AUDIO | TYPE_VIDEO);
            }
            if (bandItem->node->audio_groupID[0] != '\0') {
                mediaItem = _init_session_mediaItem(s, TYPE_AUDIO, bandItem->node->audio_groupID, NULL);
                typeMask &= ~TYPE_AUDIO;
                s->media_item_array[s->media_item_num++] = mediaItem;
            }
            if (bandItem->node->video_groupID[0] != '\0') {
                mediaItem = _init_session_mediaItem(s, TYPE_VIDEO, bandItem->node->video_groupID, NULL);
                typeMask &= ~TYPE_VIDEO;
                s->media_item_array[s->media_item_num++] = mediaItem;
            }
            if (bandItem->node->sub_groupID[0] != '\0') {
                mediaItem = _init_session_mediaItem(s, TYPE_SUBS, bandItem->node->sub_groupID, NULL);
                typeMask &= ~TYPE_SUBS;
                s->media_item_array[s->media_item_num++] = mediaItem;
            }
            mediaItem = _init_session_mediaItem(s, typeMask, NULL, bandItem->node->fileUrl);
            s->media_item_array[s->media_item_num++] = mediaItem;

            int cache_size_max = 1024 * 1024 * 10; //10M
            if (am_getconfig_bool_def("media.amplayer.low_ram", 0)) {
                cache_size_max = cache_size_max / 4;
            }

            int i;
            for (i = 0; i < s->media_item_num; i++) {
#ifdef USE_SIMPLE_CACHE
                if (s->media_item_array[i]->media_type > TYPE_VIDEO) {
                    ret = hls_simple_cache_alloc(cache_size_max / 10, &s->media_item_array[i]->media_cache);
                } else {
                    ret = hls_simple_cache_alloc(cache_size_max, &s->media_item_array[i]->media_cache);
                }
                if (ret != 0) {
                    LOGE("[%s:%d] Could not alloc buffer !", __FUNCTION__, __LINE__);
                    return -1;
                }
#endif
                // if no URI in mediaItem, TYPE_AUDIO in general, it mix with video in EXT-X-STREAM-INF.
                if (s->media_item_array[i]->media_url[0] != '\0') {
                    void * playlist = _fetch_play_list(s->media_item_array[i]->media_url, s, s->media_item_array[i], &unchanged, bandwidthIndex);
                    if (playlist == NULL || !m3u_get_node_num(playlist)) {
                        LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, s->media_item_array[i]->media_url);
                        return -1;
                    }
                    s->prev_bandwidth_index = bandwidthIndex;
                    s->media_item_array[i]->media_cur_bandwidth_index = bandwidthIndex;
                    s->media_item_array[i]->media_playlist = playlist;
                    _set_session_para(s, s->media_item_array[i]);
                }
            }
            return 0;
        }

    }

    _set_session_para(s, NULL);

    return 0;

}

static void _thread_wait_timeUs(M3ULiveSession* s, SessionMediaItem * item, int microseconds);
#define REFRESH_PLAYLIST_THRESHOLD 3
#define RINSE_REPEAT_FAILED_MAX 5

static char* _get_stbid_string(const char* url)
{
    char* begin = strstr(url, "stbId=");
    if (!begin) {
        return NULL;
    }
    char* end = strstr(begin, "&");
    char* stb = NULL;
    if (begin && end) {
        int len = end - begin - 5;
        stb = (char *)malloc(len);
        strncpy(stb, begin + 6, len - 1);
        stb[len - 1] = '\0';
    }
    return stb;
}

static char* _get_formatted_time(M3ULiveSession* s, int flag)
{
    struct tm* tm_now;
    time_t lst = 0;
    if (flag == 0) {
        lst = time(NULL) - s->refresh_point / 1000000;
    } else {
        lst = s->last_timepoint + m3u_get_durationUs(s->playlist) / 1000000 - 20;
    }
    s->last_timepoint = lst;
    tm_now = localtime(&lst);
    char strTime[100];
    strftime(strTime, sizeof(strTime), TIMESHIFT_URL_STARTTIME, tm_now);
    int slen = strlen(strTime) + 1;
    char* p = malloc(slen);
    *(p + slen - 1) = '\0';
    strcpy(p, strTime);
    return p;
}

static int _timeshift_check_refresh(M3ULiveSession* s, int* seq_num)
{
    if (s->last_timepoint <= 0) {
        return 1;
    }

    time_t now = time(NULL) - s->seektimeUs / 1000000;
    if (now > s->last_timepoint_end || now < s->last_timepoint) {
        LOGI("timeshift out of range!\n");
        return 1;
    }

    M3uBaseNode* node = NULL;
    node = m3u_get_node_by_time(s->playlist, (int64_t)((now - s->last_timepoint) * 1000000));
    if (node) {
        *seq_num = node->index + m3u_get_node_by_index(s->playlist, 0)->media_sequence;
    } else {
        LOGI("No valid segment!\n");
        return 1;
    }

    return 0;
}

static int _timeshift_refresh_playlist(M3ULiveSession* s, int bw_index, int flag)
{
    void* new_playlist = NULL;

    int64_t nowUs = get_clock_monotonic_us();
    LOGV("Prev bandwidth index:%d,current bandwidth index:%d\n", s->prev_bandwidth_index, bw_index);

    char* url = NULL;
    if (s->bandwidth_item_num > 0) {
        url = s->bandwidth_list[bw_index]->redirect != NULL ? s->bandwidth_list[bw_index]->redirect : s->bandwidth_list[bw_index]->url;
    } else {
        url = s->redirectUrl != NULL ? s->redirectUrl : s->baseUrl;
    }
    char* tmp = _get_formatted_time(s, flag);
    char shift_url[MAX_URL_SIZE];
    snprintf(shift_url, MAX_URL_SIZE, TIMESHIFT_URL_TAG, url, tmp);
    free(tmp);
    char* lm_str = strstr(shift_url, "livemode=1");
    if (lm_str) {
        *(lm_str + 9) = '2';
    }
    if (s->log_level >= HLS_SHOW_URL) {
        LOGI("timeshift request url: %s\n", shift_url);
    }
    int dummy;
    new_playlist = _fetch_play_list(shift_url, s, NULL, &dummy, bw_index);
    if (new_playlist == NULL) {
        if (s->log_level >= HLS_SHOW_URL) {
            LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, url);
        }
        s->last_bandwidth_list_fetch_timeUs = -1;
        return -1;
    }
    int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(new_playlist, 0)->media_sequence;
    if (firstSeqNumberInPlaylist == -1) {
        firstSeqNumberInPlaylist = 0;
    }
    s->cur_seq_num = firstSeqNumberInPlaylist;
    if (flag == 1) {
        int total_num = m3u_get_node_num(new_playlist);
        int i = 0;
        char * compare = strstr(s->last_timeshift_segment_url, ".ts");
        char * tmp1 = malloc(compare - s->last_timeshift_segment_url + 1);
        strncpy(tmp1, s->last_timeshift_segment_url, compare - s->last_timeshift_segment_url);
        *(tmp1 + (compare - s->last_timeshift_segment_url)) = '\0';
        char * reverse = in_strrstr(tmp1, "_");
        char * tmp2 = malloc(reverse - tmp1 + 1);
        strncpy(tmp2, tmp1, reverse - tmp1);
        *(tmp2 + (reverse - tmp1)) = '\0';
        for (i = 0; i < total_num; i++) {
            M3uBaseNode* node = m3u_get_node_by_index(new_playlist, i);
            if (node) {
                if (strstr(node->fileUrl, tmp1) != NULL) {
                    s->cur_seq_num = firstSeqNumberInPlaylist + i + 1;
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGV("find start segment in timeshift playlist, last url= %s\n", tmp1);
                    }
                    break;
                }
            }
        }
        free(tmp1);
        free(tmp2);
    }
    s->last_bandwidth_list_fetch_timeUs = get_clock_monotonic_us();

    if (s->bandwidth_item_num > 0 && s->bandwidth_list) {
        m3u_release(s->bandwidth_list[bw_index]->playlist);
        s->bandwidth_list[bw_index]->playlist = new_playlist;
    } else { //single stream
        if (s->playlist != NULL) {
            m3u_release(s->playlist);
        }
    }
    s->playlist = new_playlist;
    s->last_timepoint_end = s->last_timepoint + m3u_get_durationUs(s->playlist) / 1000000;
    return 0;

}

static int _refresh_playlist(M3ULiveSession* s)
{
    if (s == NULL) {
        LOGE("Never open session\n");
        return -1;
    }

    int bandwidthIndex = _get_best_bandwidth_index(s, NULL);
    void* new_playlist = NULL;
rinse_repeat: {
        int64_t nowUs = get_clock_monotonic_us();
        pthread_mutex_lock(&s->session_lock);
        if (s->is_closed > 0) {
            pthread_mutex_unlock(&s->session_lock);
            return -1;
        }
        if (s->need_refresh_playlist == 1) {
            if (s->timeshift_force_refresh == 1 || s->no_new_file_can_download == 1) {
                _timeshift_refresh_playlist(s, bandwidthIndex, 1);
                s->timeshift_force_refresh = 0;
            } else if (s->seektimeUs > 0 || s->force_refresh == 1) {
                int seq_num_shift = 0;
                if (s->force_refresh == 1 || _timeshift_check_refresh(s, &seq_num_shift) > 0) {
                    _timeshift_refresh_playlist(s, bandwidthIndex, 0);
                    s->force_refresh = -1;
                } else {
                    s->cur_seq_num = seq_num_shift;
                }
            } else {
                LOGV("No need to refresh timeshift playlist\n");
            }
            pthread_mutex_unlock(&s->session_lock);
            return 0;
        }
        LOGV("Prev bandwidth index:%d,current bandwidth index:%d\n", s->prev_bandwidth_index, bandwidthIndex);

        //not have end flag,test condition
        int reserved_segment_check = 0;
        if ((s->playlist != NULL)) {
            int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(s->playlist, 0)->media_sequence;
            if (firstSeqNumberInPlaylist == -1) {
                firstSeqNumberInPlaylist = 0;
            }
            reserved_segment_check = m3u_get_node_num(s->playlist) - (s->cur_seq_num - firstSeqNumberInPlaylist);
            LOGV("Reserved segment in playlist for download,%d segments\n", reserved_segment_check);
        }
#ifdef LIVEPLAY_SEEK
        if ((s->force_refresh == 1) || (s->need_refresh_playlist == 0 && ((s->timeshift_url_switch == 0) || (_time_to_refresh_bandwidth_list(s, NULL, nowUs) && (reserved_segment_check < REFRESH_PLAYLIST_THRESHOLD))))
            || (s->playlist == NULL && s->last_bandwidth_list_fetch_timeUs < 0) //force update list
#else
        if ((s->playlist == NULL && s->last_bandwidth_list_fetch_timeUs < 0) //force update list
#endif
            || bandwidthIndex != s->prev_bandwidth_index//adaptive bandwidth bitrate policy
            || ((s->playlist != NULL) && !m3u_is_complete(s->playlist) && _time_to_refresh_bandwidth_list(s, NULL, nowUs) && (reserved_segment_check < REFRESH_PLAYLIST_THRESHOLD))) { //need refresh


            s->force_refresh = -1;
            if ((s->is_livemode != 1) && (s->playlist != NULL && m3u_is_complete(s->playlist) > 0) //vod
                && (s->bandwidth_item_num > 0) && s->bandwidth_list[bandwidthIndex]->playlist != NULL) {
                new_playlist = s->bandwidth_list[bandwidthIndex]->playlist;
                if (s->log_level >= HLS_SHOW_URL) {
                    LOGV("Just reuse old parsed playlist,index:%d,url:%s\n", bandwidthIndex, s->bandwidth_list[bandwidthIndex]->url);
                } else {
                    LOGV("Just reuse old parsed playlist,index:%d\n", bandwidthIndex);
                }
            } else {
                char* url = NULL;
                if (s->bandwidth_item_num > 0) {
                    url = s->bandwidth_list[bandwidthIndex]->redirect != NULL ?
                          s->bandwidth_list[bandwidthIndex]->redirect : s->bandwidth_list[bandwidthIndex]->url;
                    if (s->durationUs > 0) {
                        memset(s->last_bandwidth_list_hash, 0, HASH_KEY_SIZE);
                    }
                } else {
                    url = s->redirectUrl != NULL ? s->redirectUrl : s->baseUrl;

                }
                if (s->need_refresh_playlist == 0 && s->timeshift_url_switch == 0) {
                    if (s->bandwidth_item_num > 0) {
                        url = s->bandwidth_list[bandwidthIndex]->url;
                    } else {
                        url = s->baseUrl;
                    }
                    s->timeshift_url_switch = -1;
                }
                int unchanged;
                new_playlist = _fetch_play_list(url, s, NULL, &unchanged, bandwidthIndex);
                if (new_playlist == NULL) {
                    if (unchanged) {
                        // We succeeded in fetching the playlist, but it was
                        // unchanged from the last time we tried.
                        if (reserved_segment_check == 0) {
                            pthread_mutex_unlock(&s->session_lock);
                            s->last_bandwidth_list_fetch_timeUs = get_clock_monotonic_us();
                            _thread_wait_timeUs(s, NULL, 100 * 1000);
                            goto rinse_repeat;
                        }
                        if (s->log_level >= HLS_SHOW_URL) {
                            LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, url);
                        }
                        pthread_mutex_unlock(&s->session_lock);
                        _thread_wait_timeUs(s, NULL, 100 * 1000);
                        return HLSERROR(EAGAIN);

                    } else {
                        if (s->log_level >= HLS_SHOW_URL) {
                            LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, url);
                        }
                        if (s->redirectUrl) {
                            free(s->redirectUrl);
                            s->redirectUrl = NULL;
                            LOGI("del redirectUrl when download failed\n");
                            pthread_mutex_unlock(&s->session_lock);
                            _thread_wait_timeUs(s, NULL, 100 * 1000);
                            return HLSERROR(EAGAIN);
                        }
                        pthread_mutex_unlock(&s->session_lock);
                        _thread_wait_timeUs(s, NULL, 100 * 1000);
                        return -1;
                    }
                }
            }
            s->last_bandwidth_list_fetch_timeUs = get_clock_monotonic_us();

        } else {
            pthread_mutex_unlock(&s->session_lock);
            if (s->no_new_file_can_download) { /*not new file,do wait.*/
                _thread_wait_timeUs(s, NULL, 500 * 1000);
            }
            LOGV("Drop refresh playlist\n");
            return 0;

        }
        int bandwidthChanged = 0;
        int explicitDiscontinuity = 0;
        int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(new_playlist, 0)->media_sequence;
        if (firstSeqNumberInPlaylist == -1) {
            firstSeqNumberInPlaylist = 0;
        }

        if (s->need_refresh_playlist == 0) {
            if (m3u_get_node_num(new_playlist) > 3) {
                s->cur_seq_num = firstSeqNumberInPlaylist + m3u_get_node_num(new_playlist) - 3;
            } else {
                s->cur_seq_num = firstSeqNumberInPlaylist;
            }
            s->need_refresh_playlist = -1;
        }
        if (s->cur_seq_num < 0) {
            s->cur_seq_num = firstSeqNumberInPlaylist;
        }
        int32_t lastSeqNumberInPlaylist =
            firstSeqNumberInPlaylist + m3u_get_node_num(new_playlist) - 1;
        if (s->cur_seq_num < firstSeqNumberInPlaylist
            || s->cur_seq_num > lastSeqNumberInPlaylist) {//seq not in this zone
            if (s->prev_bandwidth_index != bandwidthIndex) {
                // Go back to the previous bandwidth.

                LOGI("new bandwidth does not have the sequence number "
                     "we're looking for, switching back to previous bandwidth");

                s->last_bandwidth_list_fetch_timeUs = -1;
                bandwidthIndex = s->prev_bandwidth_index;
                if (new_playlist != NULL) {
                    m3u_release(new_playlist);
                    new_playlist = NULL;
                }
                pthread_mutex_unlock(&s->session_lock);
                goto rinse_repeat;
            }
            if (!m3u_is_complete(new_playlist) && s->retries_num < RINSE_REPEAT_FAILED_MAX) {
                ++s->retries_num;
#if 0
                if (s->cur_seq_num > lastSeqNumberInPlaylist) {
                    s->last_bandwidth_list_fetch_timeUs = -1;
                    pthread_mutex_unlock(&s->session_lock);
                    if (new_playlist != NULL) {
                        m3u_release(new_playlist);
                        new_playlist = NULL;
                    }
                    //just wait
                    _thread_wait_timeUs(s, 3000000); // 3s delay
                    return HLSERROR(EAGAIN);

                }
#endif

                LOGI("Current seq number is not in range, cur_seq_num : %d, first : %d, last : %d\n", s->cur_seq_num, firstSeqNumberInPlaylist, lastSeqNumberInPlaylist);
                char * cur_stbid = _get_stbid_string(m3u_get_node_by_index(new_playlist, 0)->fileUrl);
                if (s->cur_seq_num > lastSeqNumberInPlaylist) {
                    M3uBaseNode *tmp_node = m3u_get_node_by_url(new_playlist, s->last_segment_url);
                    if (tmp_node) {
                        s->cur_seq_num = firstSeqNumberInPlaylist + tmp_node->index + 1;
                    } else {
                        if (!cur_stbid || (s->stbId_string && !strcmp(cur_stbid, s->stbId_string))) { // keep cur_seq_num when m3u wrong, for some ugly server.
                            s->cur_seq_num = firstSeqNumberInPlaylist;
                        }
                    }
                } else {
                    if (!cur_stbid || (s->stbId_string && !strcmp(cur_stbid, s->stbId_string))) {
                        s->cur_seq_num = firstSeqNumberInPlaylist;
                    }
                }
                if (cur_stbid) {
                    free(cur_stbid);
                }

#if 0
                // we've missed the boat, let's start from the lowest sequence
                // number available and signal a discontinuity.

                LOGI("We've missed the boat, restarting playback.");
                // s->cur_seq_num = lastSeqNumberInPlaylist;
                s->cur_seq_num = firstSeqNumberInPlaylist;
#endif
                explicitDiscontinuity = 1;
            }  else {
                LOGE("Cannot find sequence number %d in playlist "
                     "(contains %d - %d)",
                     s->cur_seq_num, firstSeqNumberInPlaylist,
                     firstSeqNumberInPlaylist + m3u_get_node_num(new_playlist) - 1);

                pthread_mutex_unlock(&s->session_lock);
                if (new_playlist != NULL && s->cur_seq_num > lastSeqNumberInPlaylist) {
                    m3u_release(new_playlist);
                    new_playlist = NULL;
                    return -1;
                } else {
                    explicitDiscontinuity = 1;
                    s->cur_seq_num = lastSeqNumberInPlaylist;
                    LOGE("We've missed the boat, using last seq,mabye will skip some frame for ugly hls server\n");
                }


            }
        }
        if (s->prev_bandwidth_index != bandwidthIndex) {
            bandwidthChanged = 1;
        }
        s->retries_num = 0;

        if (bandwidthChanged || explicitDiscontinuity) {
            //add discontinuity flag
            LOGI("queueing discontinuity (explicit=%d, bandwidthChanged=%d)",
                 explicitDiscontinuity, bandwidthChanged);


        }

        s->prev_bandwidth_index = bandwidthIndex;
        if (s->bandwidth_item_num > 0 && s->bandwidth_list) {
            if (s->durationUs < 1 && s->bandwidth_list[bandwidthIndex]->playlist != NULL) { //live
                m3u_release(s->bandwidth_list[bandwidthIndex]->playlist);
            }
            s->bandwidth_list[bandwidthIndex]->playlist = new_playlist;

        } else { //single stream
            if (s->playlist != NULL && s->durationUs < 1) {
                m3u_release(s->playlist);
            }
        }
        s->playlist = new_playlist;
        pthread_mutex_unlock(&s->session_lock);
        return 0;

    }

}

static int _refresh_media_playlist(M3ULiveSession * s, SessionMediaItem * mediaItem) {
    int bandwidthIndex = _get_best_bandwidth_index(s, mediaItem);
    void * new_playlist = NULL;

rinse_repeat: {
        int64_t nowUs = get_clock_monotonic_us();
        pthread_mutex_lock(&mediaItem->media_lock);
        if (s->is_closed > 0) {
            pthread_mutex_unlock(&mediaItem->media_lock);
            return -1;
        }
        LOGI("[Type : %d] Prev bandwidth index:%d, current bandwidth index:%d", mediaItem->media_type, mediaItem->media_cur_bandwidth_index, bandwidthIndex);
        int reserved_segment_check = 0;
        if (mediaItem->media_playlist != NULL) {
            int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(mediaItem->media_playlist, 0)->media_sequence;
            if (firstSeqNumberInPlaylist == -1) {
                firstSeqNumberInPlaylist = 0;
            }
            reserved_segment_check = m3u_get_node_num(mediaItem->media_playlist) - (mediaItem->media_cur_seq_num - firstSeqNumberInPlaylist);
            LOGV("[Type : %d] Reserved segment in playlist for download, %d segments\n", mediaItem->media_type, reserved_segment_check);
        }
        if ((mediaItem->media_playlist == NULL && mediaItem->media_last_fetch_timeUs < 0) //force update list
            || bandwidthIndex != mediaItem->media_cur_bandwidth_index
            || mediaItem->media_switch_anchor_timeUs >= 0
            || ((mediaItem->media_playlist != NULL) && !m3u_is_complete(mediaItem->media_playlist) && _time_to_refresh_bandwidth_list(s, mediaItem, nowUs) && (reserved_segment_check < REFRESH_PLAYLIST_THRESHOLD))) {
            char * url = NULL;
            if (bandwidthIndex == mediaItem->media_cur_bandwidth_index && mediaItem->media_switch_anchor_timeUs < 0) {
                url = mediaItem->media_redirect != NULL ? mediaItem->media_redirect : mediaItem->media_url;
            } else {
                memset(mediaItem->media_last_bandwidth_list_hash, 0, HASH_KEY_SIZE);
                int ret = _reinit_session_mediaItem(s, mediaItem, bandwidthIndex);
                if (ret) {
                    pthread_mutex_unlock(&mediaItem->media_lock);
                    return ret;
                }
                if (mediaItem->media_url[0] != '\0') {
                    url = mediaItem->media_url;
                } else {
                    pthread_mutex_unlock(&mediaItem->media_lock);
                    return 0;
                }
            }
            int unchanged;
            new_playlist = _fetch_play_list(url, s, mediaItem, &unchanged, bandwidthIndex);
            if (!new_playlist) {
                if (unchanged) {
                    // We succeeded in fetching the playlist, but it was
                    // unchanged from the last time we tried.
                    if (reserved_segment_check == 0) {
                        pthread_mutex_unlock(&mediaItem->media_lock);
                        mediaItem->media_last_fetch_timeUs = get_clock_monotonic_us();
                        _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                        goto rinse_repeat;
                    }
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGE("[%s:%d] [Type : %d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, mediaItem->media_type, url);
                    }
                    pthread_mutex_unlock(&mediaItem->media_lock);
                    _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                    return HLSERROR(EAGAIN);
                } else {
                    if (s->log_level >= HLS_SHOW_URL) {
                        LOGE("[%s:%d] [Type : %d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, mediaItem->media_type, url);
                    }
                    if (mediaItem->media_redirect) {
                        free(mediaItem->media_redirect);
                        mediaItem->media_redirect = NULL;
                        LOGI("[Type : %d] del media redirect url when download failed", mediaItem->media_type);
                        pthread_mutex_unlock(&mediaItem->media_lock);
                        _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                        return HLSERROR(EAGAIN);
                    }
                    pthread_mutex_unlock(&mediaItem->media_lock);
                    _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                    return -1;
                }
            }
            mediaItem->media_last_fetch_timeUs = get_clock_monotonic_us();
            // skip sequence judgement when select/unselect.
            if (mediaItem->media_switch_anchor_timeUs >= 0) {
                goto SKIP;
            }
        } else {
            pthread_mutex_unlock(&mediaItem->media_lock);
            if (mediaItem->media_no_new_file) { /*no new file,do wait.*/
                _thread_wait_timeUs(s, mediaItem, 100 * 1000);
            }
            LOGV("[Type : %d] Drop refresh media playlist", mediaItem->media_type);
            return 0;
        }
        int32_t firstSeqNumberInPlaylist = m3u_get_node_by_index(new_playlist, 0)->media_sequence;
        if (firstSeqNumberInPlaylist == -1) {
            firstSeqNumberInPlaylist = 0;
        }
        if (mediaItem->media_cur_seq_num < 0) {
            mediaItem->media_cur_seq_num = firstSeqNumberInPlaylist;
        }
        int32_t lastSeqNumberInPlaylist = firstSeqNumberInPlaylist + m3u_get_node_num(new_playlist) - 1;
        if (mediaItem->media_cur_seq_num < firstSeqNumberInPlaylist
            || mediaItem->media_cur_seq_num > lastSeqNumberInPlaylist) {//seq not in this zone
            if (mediaItem->media_cur_bandwidth_index != bandwidthIndex) {
                // Go back to the previous bandwidth.

                LOGI("[Type : %d] new bandwidth does not have the sequence number "
                     "we're looking for, switching back to previous bandwidth", mediaItem->media_type);

                mediaItem->media_last_fetch_timeUs = -1;
                bandwidthIndex = mediaItem->media_cur_bandwidth_index;
                if (new_playlist != NULL) {
                    m3u_release(new_playlist);
                    new_playlist = NULL;
                }
                pthread_mutex_unlock(&mediaItem->media_lock);
                goto rinse_repeat;
            }
            if (!m3u_is_complete(new_playlist) && mediaItem->media_retries_num < RINSE_REPEAT_FAILED_MAX) {
                ++mediaItem->media_retries_num;
                LOGI("[Type : %d] Current seq number is not in range, cur_seq_num : %d, first : %d, last : %d", mediaItem->media_type, mediaItem->media_cur_seq_num, firstSeqNumberInPlaylist, lastSeqNumberInPlaylist);
                if (mediaItem->media_cur_seq_num > lastSeqNumberInPlaylist) {
                    M3uBaseNode *tmp_node = m3u_get_node_by_url(new_playlist, mediaItem->media_last_segment_url);
                    if (tmp_node) {
                        mediaItem->media_cur_seq_num = firstSeqNumberInPlaylist + tmp_node->index + 1;
                    } else {
                        mediaItem->media_cur_seq_num = firstSeqNumberInPlaylist;
                    }
                } else {
                    mediaItem->media_cur_seq_num = firstSeqNumberInPlaylist;
                }
            } else {
                LOGE("[Type : %d] Cannot find sequence number %d in playlist (contains %d - %d)",
                    mediaItem->media_type, mediaItem->media_cur_seq_num, firstSeqNumberInPlaylist,
                    firstSeqNumberInPlaylist + m3u_get_node_num(new_playlist) - 1);
                pthread_mutex_unlock(&mediaItem->media_lock);
                if (new_playlist != NULL && mediaItem->media_cur_seq_num > lastSeqNumberInPlaylist) {
                    m3u_release(new_playlist);
                    new_playlist = NULL;
                    return -1;
                } else {
                    mediaItem->media_cur_seq_num = lastSeqNumberInPlaylist;
                }
            }
        }

SKIP:
        mediaItem->media_retries_num = 0;
        mediaItem->media_cur_bandwidth_index = bandwidthIndex;
        if (mediaItem->media_playlist != NULL) {
            m3u_release(mediaItem->media_playlist);
        }
        mediaItem->media_playlist = new_playlist;
        pthread_mutex_unlock(&mediaItem->media_lock);
        return 0;
    }
}

static void _thread_wait_timeUs(M3ULiveSession* s, SessionMediaItem * item, int microseconds)
{
    struct timespec outtime;

    if (microseconds > 0) {

#if !defined(__aarch64__) && defined(ANDROID)
        int64_t t = get_clock_monotonic_us() + microseconds;
#else
        int64_t t = in_gettimeUs() + microseconds;
#endif
        int ret = -1;
        if (item) {
            ret = pthread_mutex_trylock(&item->media_lock);
        } else {
            ret = pthread_mutex_trylock(&s->session_lock);
        }
        if (ret != 0) {
            LOGV("Can't get lock,use usleep\n");
            amthreadpool_thread_usleep(microseconds);
            return;
        }
        outtime.tv_sec = t / 1000000;
        outtime.tv_nsec = (t % 1000000) * 1000;

#if !defined(__aarch64__) && defined(ANDROID)
        if (item) {
            ret = pthread_cond_timedwait_monotonic_np(&item->media_cond, &item->media_lock, &outtime);
        } else {
            ret = pthread_cond_timedwait_monotonic_np(&s->session_cond, &s->session_lock, &outtime);
        }
#else
        if (item) {
            ret = pthread_cond_timedwait(&item->media_cond, &item->media_lock, &outtime);
        } else {
            ret = pthread_cond_timedwait(&s->session_cond, &s->session_lock, &outtime);
        }
#endif
        if (ret != ETIMEDOUT) {
            LOGV("timed-waiting on condition");
        }
    } else {
        if (item) {
            pthread_mutex_lock(&item->media_lock);
            pthread_cond_wait(&item->media_cond, &item->media_lock);
        } else {
            pthread_mutex_lock(&s->session_lock);
            pthread_cond_wait(&s->session_cond, &s->session_lock);
        }

    }

    if (item) {
        pthread_mutex_unlock(&item->media_lock);
    } else {
        pthread_mutex_unlock(&s->session_lock);
    }
}

static void _thread_wake_up(M3ULiveSession* s, SessionMediaItem * item)
{
    if (item) {
        pthread_mutex_lock(&item->media_lock);
        pthread_cond_broadcast(&item->media_cond);
        pthread_mutex_unlock(&item->media_lock);
    } else {
        pthread_mutex_lock(&s->session_lock);
        pthread_cond_broadcast(&s->session_cond);
        pthread_mutex_unlock(&s->session_lock);
    }
}

static M3uBaseNode* _get_m3u_node_by_index(M3ULiveSession* s, int first_seq)
{
    M3uBaseNode* node = NULL;
    node = m3u_get_node_by_index(s->playlist, s->cur_seq_num - first_seq);
    if (!node) {
        return NULL;
    }
    M3uBaseNode* last_node = NULL;
    last_node = m3u_get_node_by_url(s->playlist, s->last_segment_url);
    if (!last_node) {
        return node;
    }
    int cur_seq = first_seq + last_node->index + 1;
    if (s->cur_seq_num != cur_seq) { // media seq wrong
        LOGI("M3U media sequence wrong, cur_seq_num : %d, first_seq : %d, last_node_index : %d \n", s->cur_seq_num, first_seq, last_node->index);
        node = m3u_get_node_by_index(s->playlist, last_node->index + 1);
        if (!node) {
            return NULL;
        } else {
            s->cur_seq_num = cur_seq;
            return node;
        }
    }
    return node;
}

#define READ_ONCE_BLOCK_SIZE   1024*8
static int _fetch_segment_file(M3ULiveSession* s, SessionMediaItem * mediaItem, M3uBaseNode* segment, int isLive)
{
    int ret = -1;
    void * handle = NULL;
    const char * url = segment->fileUrl;
    long long range_offset = segment->range_offset;
    long long range_length = segment->range_length;
    int indexInPlaylist = segment->index;
    int segmentDurationUs = segment->durationUs;
    s->cached_data_timeUs = segment->startUs;
    int64_t fetch_start, fetch_end;
    fetch_start = get_clock_monotonic_us();
    int drop_estimate_bw = 1;
    int need_retry = 0;
    int need_notify = 0;
    char * cookies = NULL;
    void * cache = NULL;
    int is_encrypt_media = 0;
    if (mediaItem) {
        is_encrypt_media = mediaItem->media_encrypted;
    } else {
        is_encrypt_media = s->is_encrypt_media;
    }
    if (!mediaItem) {
        if (((URLContext *)(s->urlcontext))->notify_id != -1 && strstr(s->baseUrl, "diagnose=deep")) {
            need_notify = 1;
        }
    }
    if (need_notify) {
        LOGV("notify fetch start\n");
        ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_START, s->cur_seq_num, 0);
    }

open_retry: {
        if (s->is_closed > 0 || (s->seekflag > 0 || (mediaItem && mediaItem->media_seek_flag == 2)) || (s->interrupt && (*s->interrupt)())) {
            LOGV("Get close flag before opening,(value:%d) or seek flag(value:%d)\n", s->is_closed, s->seekflag);
            ret = -3;
            if (s->seekflag == 2) {
                ret = 0;
            }
            if (s->seekflag > 0) {
                if (mediaItem) {
                    mediaItem->media_seek_flag = 0;
                } else {
                    s->seekflag = 0;
                }
            }
            if (mediaItem && mediaItem->media_seek_flag == 2) {
                mediaItem->media_seek_flag = 0;
            }
            return ret;
        }

        char headers[MAX_URL_SIZE] = {0};
        if (range_offset >= 0) {
            int pos = 0;
            if (s->headers != NULL) {
                snprintf(headers, MAX_URL_SIZE, "%s\r\n", s->headers);
                pos = strlen(s->headers);
            }
            char str[32];
            int64_t len = range_offset + range_length - 1;
            snprintf(str, 32, "%lld", len);
            *(str + strlen(str) + 1) = '\0';
            snprintf(headers + pos, MAX_URL_SIZE - pos, "Range: bytes=%lld-%s", (long long)range_offset, range_length <= 0 ? "" : str);
            if (in_get_sys_prop_bool("media.libplayer.curlenable") <= 0 || !strstr(url, "https://")) {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\n");
            }
            if (s->log_level >= HLS_SHOW_URL) {
                LOGV("Got headers:%s\n", headers);
            }

        } else {
            if (s->headers != NULL) {
                strncpy(headers, s->headers, MAX_URL_SIZE);
            }
        }

        if (need_retry == 1) { // segment ts maybe put on http server not https
            if (strcasestr(url, "https")) {
                char tmp_url[MAX_URL_SIZE];
                snprintf(tmp_url, MAX_URL_SIZE, "http%s", segment->fileUrl + 5);
                url = tmp_url;
            }
            need_retry = 0;
        }

        if (mediaItem) {
            cookies = mediaItem->media_cookies;
            cache = mediaItem->media_cache;
        } else {
            cookies = s->cookies;
            cache = s->cache;
        }
        if (cookies && strlen(cookies) > 0) {
            if (s->headers != NULL && strlen(s->headers) > 0 && s->headers[strlen(s->headers) - 1] != '\n') {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", cookies);
            } else {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", cookies);
            }
        }

        if (is_encrypt_media > 0) {
            // TODO: add media group decrypt logic here.
            AESKeyInfo_t keyinfo;
            ret = _get_decrypt_key(s, mediaItem, indexInPlaylist, &keyinfo);
            if (ret != 0) {
                _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                handle = NULL;
                goto open_retry;
            }
            ret = hls_http_open(url, headers, (void*)&keyinfo, &handle);
            if (keyinfo.key_info != NULL) {
                free(keyinfo.key_info);
            }
        } else {
            ret = hls_http_open(url, headers, NULL, &handle);
        }
        int errcode = 0;
        if (ret != 0) {
            errcode = hls_http_get_error_code(handle);
            if (errcode == -800) {
                if (s->log_level >= HLS_SHOW_URL) {
                    LOGV("Maybe seek play,just retry to open,url:%s\n", url);
                }
                _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                hls_http_close(handle);
                handle = NULL;
                goto open_retry;
            }
            int64_t now = get_clock_monotonic_us();
            if (!isLive && ((now - fetch_start) < segmentDurationUs * 10)) { //maybe 10s*10 = 100s.
                if (s->log_level >= HLS_SHOW_URL) {
                    LOGV("[VOD]Just retry to open,url:%s,max retry time:%d s\n", url, segmentDurationUs / 100000);
                } else {
                    LOGV("[VOD]Just retry to open,max retry time:%d s\n", segmentDurationUs / 100000);
                }
                _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                hls_http_close(handle);
                handle = NULL;
                need_retry = 1;
                goto open_retry;

            } else if (isLive && (now - fetch_start) < segmentDurationUs / 2) { //maybe 5s
                if (s->log_level >= HLS_SHOW_URL) {
                    LOGV("[LIVE]Just retry to open,url:%s,max retry time:%d s\n", url, segmentDurationUs / 2000000);
                } else {
                    LOGV("[LIVE]Just retry to open,max retry time:%d s\n", segmentDurationUs / 2000000);
                }
                _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                hls_http_close(handle);
                handle = NULL;
                need_retry = 1;
                goto open_retry;
            } else { //failed to download,need skip this file
                LOGI("[%s],skip this segment\n", isLive > 0 ? "LIVE" : "VOD");
                if (mediaItem) {
                    mediaItem->media_err_code = errcode < 0 ? (-errcode) : (-ret);
                } else {
                    s->err_code = errcode < 0 ? (-errcode) : (-ret); //small trick,avoid to exit player read logic
                }
                hls_http_close(handle);
                handle = NULL;
                if (need_notify) {
                    LOGV("notify fetch error\n");
                    int code = (-errcode == 408) ? 2000 : (-errcode);
                    s->last_notify_err_seq_num = s->cur_seq_num;
                    ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_ERROR, code, s->cur_seq_num);
                }
                return HLSERROR(EAGAIN);
            }

        }

        //allocate cache node
        long long fsize = hls_http_get_fsize(handle);
        int64_t lastreadtime_us = get_clock_monotonic_us();
        LOGV("Get segment file size:%lld\n", fsize);
        long long read_size = 0;
        int buf_tmp_size = READ_ONCE_BLOCK_SIZE * 8;
        if (fsize > 0) {
            buf_tmp_size = HLSMIN(fsize, READ_ONCE_BLOCK_SIZE * 8);
            LOGV("Temp buffer size:%d\n", buf_tmp_size);
            segment->range_length = fsize;
        }

        if (segment->durationUs > 0 && segment->range_length > 0) {
            if (mediaItem) {
                mediaItem->media_estimate_bps = (double)(segment->range_length * 8 * 1000000) / (double)segment->durationUs;
            } else {
                s->stream_estimate_bps = (double)(segment->range_length * 8 * 1000000) / (double)segment->durationUs;
            }
        }

        unsigned char buf_tmp[buf_tmp_size];
        int buf_tmp_rsize = 0;
        int is_add_ts_fake_head = _is_add_fake_leader_block(s);
        if (mediaItem && mediaItem->media_type > TYPE_VIDEO) {
            is_add_ts_fake_head = -1;
        }

        for (;;) {
            if (s->interrupt && (*s->interrupt)()) {
                LOGV("[%s:%d]: interrupted! \n", __FUNCTION__, __LINE__);
                hls_http_close(handle);
                return 0;
            }

            if (s->is_closed || (s->seekflag > 0 || (mediaItem && mediaItem->media_seek_flag == 2))) {
                LOGV("Get close flag(value:%d) or seek flag(value:%d)\n", s->is_closed, s->seekflag);
                ret = -3;
                if (s->seekflag == 2) {
                    ret = 0;
                }
                if (s->seekflag > 0) {
                    if (mediaItem) {
                        mediaItem->media_seek_flag = 0;
                    } else {
                        s->seekflag = 0;
                    }
                }
                if (mediaItem && mediaItem->media_seek_flag == 2) {
                    mediaItem->media_seek_flag = 0;
                }
                hls_http_close(handle);
                return ret;
            }

            int rlen = 0;
#ifdef USE_SIMPLE_CACHE
            int extra_size = 0;
            if (is_add_ts_fake_head > 0) {
                extra_size = 188;
            }
            if (hls_simple_cache_get_free_space(cache) < (buf_tmp_size + extra_size)) {
                LOGV("Simple cache not have free space,just wait\n");
                _thread_wait_timeUs(s, mediaItem, 500 * 1000);
                drop_estimate_bw = 1;
                if (mediaItem) {
                    mediaItem->media_monitor_timer = get_clock_monotonic_us();
                } else {
                    s->download_monitor_timer = get_clock_monotonic_us();
                }
                continue;
            }
#endif
            if (is_add_ts_fake_head > 0) {
                unsigned char fbuf[188];
                _generate_fake_ts_leader_block(fbuf, 188, segmentDurationUs / 1000);
#ifdef USE_SIMPLE_CACHE
                hls_simple_cache_write(cache, fbuf, 188);
#endif
                is_add_ts_fake_head = 0;
            }
            rlen = hls_http_read(handle, buf_tmp + buf_tmp_rsize, HLSMIN(buf_tmp_size - buf_tmp_rsize, READ_ONCE_BLOCK_SIZE));
            int est_bps = 0;
            hls_http_estimate_bandwidth(handle, &est_bps);
            if (mediaItem) {
                mediaItem->media_estimate_bandwidth_bps = est_bps;
                if (rlen == HLSERROR(EINTR) && (s->seekflag > 0 || mediaItem->media_seek_flag == 2)) {
                    mediaItem->media_seek_flag = 0;
                }
            } else {
                s->estimate_bandwidth_bps = est_bps;
                if (rlen == HLSERROR(EINTR) && s->seekflag > 0) {
                    s->seekflag = 0;
                }
            }

            if (rlen > 0) {
                buf_tmp_rsize += rlen;
                read_size += rlen;
                if (fsize > 0) {
                    s->cached_data_timeUs = segment->startUs + segmentDurationUs * read_size / fsize;
                }
                if (!(mediaItem && mediaItem->media_type > TYPE_VIDEO)) {
                    if (s->is_ts_media == -1 && rlen > 188) {
                        if (_ts_simple_analyze(buf_tmp, HLSMIN(buf_tmp_rsize, 188)) > 0) {
                            s->is_ts_media = 1;
                        } else {
                            s->is_ts_media = 0;
                        }
                    }
                }
                if (buf_tmp_rsize >= buf_tmp_size) { //first node
                    //LOGV("Move temp buffer data to cache");

#ifdef USE_SIMPLE_CACHE

                    hls_simple_cache_write(cache, buf_tmp, buf_tmp_rsize);
#endif
                    buf_tmp_rsize = 0;
                }

                if (fsize > 0 && read_size >= fsize) {
                    LOGV("Maybe reached EOS,force to exit it\n");
                    if (buf_tmp_rsize > 0) {

#ifdef USE_SIMPLE_CACHE
                        hls_simple_cache_write(cache, buf_tmp, buf_tmp_rsize);
#endif
                        buf_tmp_rsize = 0;
                    }
                    hls_http_close(handle);
                    handle = NULL;
                    if (drop_estimate_bw == 0) {
                        fetch_end = get_clock_monotonic_us();
                        bandwidth_measure_add(s->bw_meausure_handle, fsize, fetch_end - fetch_start);
                    }
                    if (need_notify) {
                        LOGV("notify fetch end\n");
                        ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_END, (int)((in_gettimeUs() - fetch_start) / 1000), s->cur_seq_num);
                    }
                    return 0;
                }
                lastreadtime_us = get_clock_monotonic_us();
            } else if (rlen == 0) {

                if (buf_tmp_rsize > 0) {

#ifdef USE_SIMPLE_CACHE
                    hls_simple_cache_write(cache, buf_tmp, buf_tmp_rsize);
#endif
                    buf_tmp_rsize = 0;
                }
                if (fsize < 1) { //for chunk streaming,can't get filesize
                    segment->range_length = read_size;
                }
                hls_http_close(handle);
                handle = NULL;
                if (drop_estimate_bw == 0) {
                    fetch_end = get_clock_monotonic_us();
                    bandwidth_measure_add(s->bw_meausure_handle, read_size, fetch_end - fetch_start);
                }
                if (need_notify) {
                    LOGV("notify fetch end\n");
                    ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_END, (int)((in_gettimeUs() - fetch_start) / 1000), s->cur_seq_num);
                }
                return 0;
            } else {
                int64_t failed_curtime_us = get_clock_monotonic_us();
                if (fsize > 0 && read_size >= fsize) {

#ifdef USE_SIMPLE_CACHE
                    if (buf_tmp_rsize > 0) {
                        hls_simple_cache_write(cache, buf_tmp, buf_tmp_rsize);
                        buf_tmp_rsize = 0;
                    }
#endif

                    LOGV("Http return error value,maybe reached EOS,force to exit it\n");
                    hls_http_close(handle);
                    handle = NULL;
                    if (drop_estimate_bw == 0) {
                        fetch_end = get_clock_monotonic_us();
                        bandwidth_measure_add(s->bw_meausure_handle, read_size, fetch_end - fetch_start);
                    }
                    if (need_notify) {
                        LOGV("notify fetch end\n");
                        ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_END, (int)((in_gettimeUs() - fetch_start) / 1000), s->cur_seq_num);
                    }
                    return 0;
                }
                if (rlen == HLSERROR(EAGAIN)) {
                    LOGV("Read retry. live=%d outtime=%lld\n", isLive, failed_curtime_us - lastreadtime_us);
                    if (isLive == 0 && get_clock_monotonic_us() > lastreadtime_us + 5 * 1000 * 1000) { //about 5s
                        hls_http_close(handle);
                        return rlen;
                    }
                    if (isLive > 0 && (failed_curtime_us - lastreadtime_us) > 1 * 1000 * 1000
                        && (failed_curtime_us - fetch_start) > segmentDurationUs / 2) {
                        /*segmentDurationUs /3  not get any data,reopen it.*/
                        hls_http_close(handle);
                        handle = NULL;
                        return 0;
                    }
                    _thread_wait_timeUs(s, mediaItem, 100 * 1000);
                    continue;
                }
                if (isLive > 0 || rlen == HLSERROR(EINTR) || rlen == HLSERROR(ENETRESET) || rlen == HLSERROR(ECONNRESET)) { //live streaming,skip current segment
                    if (mediaItem) {
                        mediaItem->media_err_code = -rlen;
                    } else {
                        s->err_code = -rlen;//small trick
                    }
                    hls_http_close(handle);
                    return HLSERROR(EAGAIN);
                } else {
                    if (mediaItem) {
                        mediaItem->media_err_code = rlen;
                    } else {
                        s->err_code = rlen;//small trick
                    }
                    hls_http_close(handle);
                    return rlen;
                }

            }
        }

        return 0;
    }
}
static int _download_next_segment(M3ULiveSession* s)
{
    if (s == NULL) {
        LOGE("Sanity check\n");
        return -2;
    }

    int32_t firstSeqNumberInPlaylist = -1;

    pthread_mutex_lock(&s->session_lock);

    if (s->playlist != NULL) {

        firstSeqNumberInPlaylist = m3u_get_node_by_index(s->playlist, 0)->media_sequence;

    } else {
        LOGE("Can't find playlist,need refresh playlist\n");
        pthread_mutex_unlock(&s->session_lock);
        return 0;

    }
    if (firstSeqNumberInPlaylist == -1) {
        firstSeqNumberInPlaylist = 0;
    }
    if (s->seektimeUs < 0 && (s->cur_seq_num - firstSeqNumberInPlaylist > m3u_get_node_num(s->playlist) - 1)) {
        LOGI("Can't find valid segment in playlist,need refresh playlist\n");
        s->no_new_file_can_download = 1;
        pthread_mutex_unlock(&s->session_lock);
        return 0;
    }
    int seekDiscontinuity = 1;
    M3uBaseNode* node = NULL;
    int seek_by_pos = -1;
    s->no_new_file_can_download = 0;
    if (s->seektimeUs >= 0) {

        if (s->is_livemode == 1) {
#ifdef USE_SIMPLE_CACHE
            hls_simple_cache_reset(s->cache);
#endif
            seekDiscontinuity = 1;
        } else if (m3u_is_complete(s->playlist) > 0) {
            node = m3u_get_node_by_time(s->playlist, s->seektimeUs);
            if (node) {
                int32_t newSeqNumber = firstSeqNumberInPlaylist + node->index;

                if (newSeqNumber != s->cur_seq_num || s->seekflag == 2) { //flag is 2,force seek
                    LOGI("seeking to seq no %d", newSeqNumber);

                    s->cur_seq_num = newSeqNumber;

                    //reset current download cache node
#ifdef USE_SIMPLE_CACHE
                    hls_simple_cache_reset(s->cache);
#endif
                    seekDiscontinuity = 1;
                }
            }

        }
#if 0
        if ((s->seektimeUs - node->startUs) > HLSMIN(2 * 1000000, node->durationUs / 3) && node->range_length > 0) { // >3s
            seek_by_pos = (s->seektimeUs - node->startUs) * node->range_length / node->durationUs;
            LOGV("Got seek pos:%d in segment\n", seek_by_pos);
        }
#endif
        s->seektimeUs = -1;

    }
    int isLive = m3u_is_complete(s->playlist) > 0 ? 0 : 1;
    if (node == NULL) {
        if (isLive && strstr(s->baseUrl, "ysten.com")) { // just for ysten server now, fix wrong media sequence.
            node = _get_m3u_node_by_index(s, firstSeqNumberInPlaylist);
        } else {
            node = m3u_get_node_by_index(s->playlist, s->cur_seq_num - firstSeqNumberInPlaylist);
        }
    }
    char* stb = NULL;
    if (node == NULL || ((stb = _get_stbid_string(node->fileUrl)) && s->stbId_string && strcmp(stb, s->stbId_string))) {
        LOGE("Can't find valid segment in playlist,need refresh playlist,seq:%d\n", s->cur_seq_num);
        pthread_mutex_unlock(&s->session_lock);
        if (!node) {
            _thread_wait_timeUs(s, NULL, 100 * 1000);
            s->force_refresh = 1;
        } else {
            s->force_refresh = 1;
            if (stb) {
                free(stb);
            }
        }
        return HLSERROR(EAGAIN);

    }
    if (stb) {
        free(stb);
    }


#if 0
    if (s->seekflag > 0 && s->seekposByte > 0) {
        seek_by_pos = s->seekposByte;
        s->seekposByte = -1;
    }
#else
    //LOGV("--------------- seekflag = %d \n", s->seekflag);
    if (s->seekflag >= 0) { // seek by byte
        //LOGV("_download_next_segment: seek_by_pos = %d, s->seekposByte = %lld, s->seektimeUs = %lld\n", seek_by_pos, s->seekposByte, s->seektimeUs);
        if (s->seekposByte > 0) {
            seek_by_pos = s->seekposByte;
            s->seekposByte = -1;
        } else if (s->seektimeUs > 0) {
            int64_t length = 0;
            void* handle = NULL;

            if ((s->seektimeUs - node->startUs) > POS_SEEK_THRESHOLD) {
                //LOGV("_download_next_segment: node->fileUrl = %s \n", node->fileUrl);
                char headers[MAX_URL_SIZE] = {0};
                if (s->headers != NULL) {
                    strncpy(headers, s->headers, MAX_URL_SIZE);
                }
                if (s->cookies && strlen(s->cookies) > 0) {
                    if (s->headers != NULL && strlen(s->headers) > 0 && s->headers[strlen(s->headers) - 1] != '\n') {
                        snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", s->cookies);
                    } else {
                        snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", s->cookies);
                    }
                }
                int rv = hls_http_open(node->fileUrl, headers, NULL, &handle);

                LOGV("_download_next_segment: rv = %d \n", rv);
                LOGV("_download_next_segment: s->seektimeUs = %lld, node->startUs = %lld \n", s->seektimeUs, node->startUs);
                if (rv != 0) {
                    if (handle != NULL) {
                        hls_http_close(handle);
                    }
                    return -1;
                }

                if (handle != NULL) {
                    length = hls_http_get_fsize(handle);
                    LOGV("_download_next_segment: length = %lld \n", length);
                    hls_http_close(handle);
                }
            }

            if (length > 0) {
                seek_by_pos = (int)(length * ((s->seektimeUs - node->startUs - POS_SEEK_THRESHOLD) / (float)(node->durationUs)));
                LOGV("seek to node num :%d,  pos : %d, seektimeUs : %lld, startUs : %lld \n", node->index, seek_by_pos, s->seektimeUs, node->startUs);
            }
            s->seektimeUs = -1;
        }
    }
#endif

    M3uBaseNode segment;
    memcpy((void*)&segment, node, sizeof(M3uBaseNode));
    segment.media_sequence = s->cur_seq_num;
    if (s->seekflag > 0 && seek_by_pos > 0) {
        segment.range_offset = seek_by_pos;
        segment.range_length = -1;
    }
    M3uKeyInfo keyInfo;
    if (segment.flags & CIPHER_INFO_FLAG) {
        memcpy((void*)&keyInfo, node->key, sizeof(M3uKeyInfo));
        segment.key = &keyInfo;

    }
    if (s->need_refresh_playlist > 0) {
        isLive = 0;
    }
    if (s->seekflag > 0) {
        s->seekflag = 0;
    }
    pthread_mutex_unlock(&s->session_lock);


    int ret = -1;
    if (s->log_level >= HLS_SHOW_URL) {
        LOGI("start fetch segment file,url:%s,seq:%d, first=%d\n", segment.fileUrl, s->cur_seq_num, firstSeqNumberInPlaylist);
    } else {
        LOGI("start fetch segment file,seq:%d\n", s->cur_seq_num);
    }
    // need to remember node url to prevent media sequence jitter
    if (isLive) {
        if (s->last_segment_url) {
            free(s->last_segment_url);
        }
        s->last_segment_url = strdup(node->fileUrl);
    }
    ret = _fetch_segment_file(s, NULL, &segment, isLive);
    if (segment.range_length > 0) {
        node->range_length = segment.range_length;
        LOGV("Got segment size:%lld\n", node->range_length);
        if (node->durationUs > 0 && s->bandwidth_item_num == 0) {
            s->stream_estimate_bps = (double)(node->range_length * 8 * 1000000) / (double)node->durationUs;
        }
    }

    if ((ret == 0 || ret == -1000 || ret == HLSERROR(EAGAIN)) && s->seekflag <= 0) { //must not seek
        pthread_mutex_lock(&s->session_lock);
        if (s->is_livemode == 1 && s->need_refresh_playlist == 1) {
            if (s->cur_seq_num - firstSeqNumberInPlaylist == m3u_get_node_num(s->playlist) - 1) {
                if (s->last_timeshift_segment_url) {
                    free(s->last_timeshift_segment_url);
                }
                s->last_timeshift_segment_url = strdup(node->fileUrl);
                s->timeshift_force_refresh = 1;
            }
        }
        ++s->cur_seq_num;
        pthread_mutex_unlock(&s->session_lock);
    }
    return ret;

}

// media group download
static int _download_media_next_segment(M3ULiveSession * s, SessionMediaItem * mediaItem) {
    if (s == NULL || mediaItem == NULL) {
        LOGE("Sanity check");
        return -2;
    }
    int32_t firstSeqNumberInPlaylist = -1;
    pthread_mutex_lock(&mediaItem->media_lock);
    if (mediaItem->media_playlist) {
        firstSeqNumberInPlaylist = m3u_get_node_by_index(mediaItem->media_playlist, 0)->media_sequence;
    } else {
        LOGE("[Type : %d] Can't find playlist, need refresh playlist", mediaItem->media_type);
        pthread_mutex_unlock(&mediaItem->media_lock);
        return 0;
    }
    if (firstSeqNumberInPlaylist == -1) {
        firstSeqNumberInPlaylist = 0;
    }
    if (mediaItem->media_seek_timeUs < 0 && mediaItem->media_switch_anchor_timeUs < 0 && (mediaItem->media_cur_seq_num - firstSeqNumberInPlaylist > m3u_get_node_num(mediaItem->media_playlist) - 1)) {
        LOGE("[Type : %d] Can't find valid segment in playlist, seq : %d", mediaItem->media_type, mediaItem->media_cur_seq_num);
        mediaItem->media_no_new_file = 1;
        pthread_mutex_unlock(&mediaItem->media_lock);
        return 0;
    }
    mediaItem->media_no_new_file = 0;
    M3uBaseNode * node = NULL;
    int64_t tmp_timeUs = -1;
    if (mediaItem->media_switch_anchor_timeUs >= 0) {
        tmp_timeUs = mediaItem->media_switch_anchor_timeUs;
    } else if (mediaItem->media_seek_timeUs >= 0) {
        tmp_timeUs = mediaItem->media_seek_timeUs;
    }
    if (tmp_timeUs >= 0) {
        if (m3u_is_complete(mediaItem->media_playlist) > 0) {
            node = m3u_get_node_by_time(mediaItem->media_playlist, tmp_timeUs);
            if (node) {
                int32_t newSeqNumber = firstSeqNumberInPlaylist + node->index;
                if (newSeqNumber != mediaItem->media_cur_seq_num || s->seekflag == 2) { //flag is 2,force seek
                    LOGI("[Type : %d] seeking to seq no %d", mediaItem->media_type, newSeqNumber);
                    mediaItem->media_cur_seq_num = newSeqNumber;
#ifdef USE_SIMPLE_CACHE
                    hls_simple_cache_reset(mediaItem->media_cache);
#endif
                }
            }
        }
        if (mediaItem->media_switch_anchor_timeUs >= 0) {
            mediaItem->media_switch_anchor_timeUs = -1;
        } else {
            mediaItem->media_seek_timeUs = -1;
        }
    }
    int isLive = m3u_is_complete(mediaItem->media_playlist) > 0 ? 0 : 1;
    if (node == NULL) {
        node = m3u_get_node_by_index(mediaItem->media_playlist, mediaItem->media_cur_seq_num - firstSeqNumberInPlaylist);
    }
    if (node == NULL) {
        LOGE("[Type : %d] Can't find valid segment in playlist,need refresh playlist,seq:%d", mediaItem->media_type, mediaItem->media_cur_seq_num);
        pthread_mutex_unlock(&mediaItem->media_lock);
        _thread_wait_timeUs(s, mediaItem, 100 * 1000);
        return HLSERROR(EAGAIN);
    }
    M3uBaseNode segment;
    memcpy((void*)&segment, node, sizeof(M3uBaseNode));
    segment.media_sequence = mediaItem->media_cur_seq_num;
    M3uKeyInfo keyInfo;
    if (segment.flags & CIPHER_INFO_FLAG) {
        memcpy((void*)&keyInfo, node->key, sizeof(M3uKeyInfo));
        segment.key = &keyInfo;
    }
    if (s->seekflag > 0 || mediaItem->media_seek_flag == 2) {
        mediaItem->media_seek_flag = 0;
    }
    pthread_mutex_unlock(&mediaItem->media_lock);
    int ret = -1;
    if (s->log_level >= HLS_SHOW_URL) {
        LOGI("[Type : %d] start fetch segment file,url:%s,seq:%d, first=%d", mediaItem->media_type, segment.fileUrl, mediaItem->media_cur_seq_num, firstSeqNumberInPlaylist);
    } else {
        LOGI("[Type : %d] start fetch segment file,seq:%d", mediaItem->media_type, mediaItem->media_cur_seq_num);
    }
    // need to remember node url to prevent media sequence jitter
    if (isLive) {
        if (mediaItem->media_last_segment_url) {
            free(mediaItem->media_last_segment_url);
        }
        mediaItem->media_last_segment_url = strdup(node->fileUrl);
    }
    ret = _fetch_segment_file(s, mediaItem, &segment, isLive);
    if (segment.range_length > 0) {
        node->range_length = segment.range_length;
        LOGV("[Type : %d] Got segment size:%lld\n", mediaItem->media_type, node->range_length);
        if (node->durationUs > 0) {
            mediaItem->media_estimate_bps = (double)(node->range_length * 8 * 1000000) / (double)node->durationUs;
        }
    }
    if ((ret == 0 || ret == -1000 || ret == HLSERROR(EAGAIN)) && (s->seekflag <= 0 && mediaItem->media_seek_flag <= 0)) { //must not seek
        pthread_mutex_lock(&mediaItem->media_lock);
        ++mediaItem->media_cur_seq_num;
        pthread_mutex_unlock(&mediaItem->media_lock);
    }
    return ret;
}

static int _synchronous_download_media(M3ULiveSession* s, SessionMediaItem * mediaItem) {
    return 0;
}

// > 0 : need to download next segment; <= 0 : sleep
static int _monitor_cached_buffer(SessionMediaItem * mediaItem) {
    if (mediaItem->media_handling_seek > 0) {
        return 1;
    }
    // download subtitle one by one.
    if (mediaItem->media_type > TYPE_VIDEO) {
        if (hls_simple_cache_get_data_size(mediaItem->media_cache) > 0) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
#if 0
    if (mediaItem->media_codec_buffer_time_s > MEDIA_CACHED_BUFFER_THREASHOLD) {
        return 0;
    } else {
        return 1;
    }
#endif
}

static int _finish_download_last(M3ULiveSession* s, SessionMediaItem * mediaItem)
{
    void * playlist = NULL;
    int cur_seq_num_tmp = 0;
    if (mediaItem) {
        playlist = mediaItem->media_playlist;
        cur_seq_num_tmp = mediaItem->media_cur_seq_num;
    } else {
        playlist = s->playlist;
        cur_seq_num_tmp = s->cur_seq_num;
    }
    if (playlist == NULL
        || m3u_is_complete(playlist) < 1
        || s->is_livemode == 1
        || (mediaItem && mediaItem->media_switch_anchor_timeUs >= 0)) {
        return 0;
    }
    int firstSeqInPlaylist = m3u_get_node_by_index(playlist, 0)->media_sequence;
    if (firstSeqInPlaylist == -1) {
        firstSeqInPlaylist = 0;
    }
    int isLast = 0;
    if (cur_seq_num_tmp - firstSeqInPlaylist >= m3u_get_node_num(playlist)) {
        s->cached_data_timeUs = s->durationUs;
        isLast = 1;
    }

    return isLast;
}

#define FAILOVER_TIME_MAX 60*60    //60min
static void* _download_worker(void* ctx)
{
    if (ctx == NULL) {
        LOGE("Sanity check\n");
        return (void*)NULL;
    }
    M3ULiveSession* s = ctx;
    int ret = -1;
    int pass = 0;
    int64_t now;
    s->download_monitor_timer = get_clock_monotonic_us();
    int failover_time = in_get_sys_prop_float("libplayer.hls.failover_time");
    failover_time = HLSMAX(failover_time, FAILOVER_TIME_MAX);
    do {
        if (pass == 1) {
            goto REFRESH;
        }
        ret = _download_next_segment(s); //100ms delay
        if (ret < 0 && ret != -3 && ret != -1000) {
            if (ret != HLSERROR(EAGAIN)) {
                break;
            }
            now = get_clock_monotonic_us();

            if ((now - s->download_monitor_timer) / 1000000 > failover_time) {
                LOGE("Can't go on playing in failover time,%d s\n", failover_time);
                if (s->err_code == 0) {
                    s->err_code = -501;
                }
                break;
            }
        } else {
            s->download_monitor_timer = get_clock_monotonic_us();
            s->err_code = 0;
        }
        if (s->is_closed > 0) {
            break;
        }

        while (s->handling_seek > 0) {
            if (s->seekflag > 0) {
                s->seekflag = 0;
            }
            amthreadpool_thread_usleep(10 * 1000);
        }
REFRESH:
        pass = 0;
        ret =  _refresh_playlist(s);
        if (ret < 0) {
            if (s->cur_seq_num != s->last_notify_err_seq_num) {
                s->last_notify_err_seq_num = s->cur_seq_num;
                if (s->is_closed < 1 && s->interrupt && !(*s->interrupt)()) {
                    ffmpeg_notify(s->urlcontext, MEDIA_INFO_DOWNLOAD_ERROR, s->err_code, s->cur_seq_num);
                }
            }
        }
        if (_finish_download_last(s, NULL) > 0) {
            if (s->err_code < 0) {
                break;
            }
	  if ( s->seektimeUs == -1 ) {
                LOGI("Download all segments,worker sleep...\n");
                s->eof_flag = 1;
                s->err_code = 0;
                _thread_wait_timeUs(s, NULL, -1);
                s->download_monitor_timer = get_clock_monotonic_us();
            } else {
                LOGI("Maybe download all segments, but has seek need to handle\n");
            }
        }
        while (s->is_closed < 1 && s->interrupt && (*s->interrupt)()) {
            if (s->handling_seek > 0) {
                break;
            }
            amthreadpool_thread_usleep(100 * 1000); //waked by close or read.
        }
        while (s->handling_seek > 0) {
            if (s->seekflag > 0) {
                s->seekflag = 0;
            }
            if (s->need_refresh_playlist >= 0) {
                pass = 1;
            }
            amthreadpool_thread_usleep(10 * 1000);
        }
    } while (s->is_closed < 1);
    if (s->err_code != 0) {
        s->err_code = -(DOWNLOAD_EXIT_CODE);
    }
    LOGI("Session download worker end,error code:%d\n", s->err_code);
    return (void*)NULL;

}

static void * _media_download_worker(void * ctx) {
    if (!ctx) {
        LOGE("Sanity check\n");
        return NULL;
    }
    int ret = -1, pass = 0;
    int64_t now;
    SessionMediaItem * mediaItem = (SessionMediaItem *)ctx;
    M3ULiveSession * session = (M3ULiveSession *)mediaItem->session;
    mediaItem->media_monitor_timer = get_clock_monotonic_us();
    int failover_time = in_get_sys_prop_float("libplayer.hls.failover_time");
    failover_time = HLSMAX(failover_time, FAILOVER_TIME_MAX);
    do {
        if (mediaItem->media_url[0] == '\0') {
            LOGI("[Type : %d] this media mix with other type, sleep...", mediaItem->media_type);
            _thread_wait_timeUs(session, mediaItem, -1);
            mediaItem->media_monitor_timer = get_clock_monotonic_us();
        }
        if (pass == 1) {
            goto REFRESH;
        }
        if (_monitor_cached_buffer(mediaItem) <= 0) {
            LOGV("[Type : %d] codec buffer had enough data, sleep...", mediaItem->media_type);
            amthreadpool_thread_usleep(100000); // 100ms
            continue;
        }
        if (mediaItem->media_type == TYPE_SUBS) {
            mediaItem->media_sub_ready = 0;
        }
        ret = _download_media_next_segment(session, mediaItem); //100ms delay
        if (ret < 0 && ret != -3 && ret != -1000) {
            if (ret != HLSERROR(EAGAIN)) {
                break;
            }
            now = get_clock_monotonic_us();
            if (mediaItem->media_type == TYPE_SUBS) {
#ifdef USE_SIMPLE_CACHE
                pthread_mutex_lock(&mediaItem->media_lock);
                hls_simple_cache_reset(mediaItem->media_cache);
                pthread_mutex_unlock(&mediaItem->media_lock);
#endif
            }
            if ((now - mediaItem->media_monitor_timer) / 1000000 > failover_time) {
                LOGE("[Type : %d] can't go on playing in failover time, %d s", mediaItem->media_type, failover_time);
                if (mediaItem->media_err_code == 0) {
                    mediaItem->media_err_code = -501;
                }
                break;
            }
        } else {
            mediaItem->media_monitor_timer = get_clock_monotonic_us();
            mediaItem->media_err_code = 0;
            if (mediaItem->media_type == TYPE_SUBS) {
                mediaItem->media_sub_ready = 1;
            }
        }
        if (session->is_closed > 0) {
            break;
        }

        while (mediaItem->media_handling_seek > 0) {
            if (mediaItem->media_seek_flag > 0) {
                mediaItem->media_seek_flag = 0;
            }
            amthreadpool_thread_usleep(10 * 1000);
        }

REFRESH:
        pass = 0;
        ret = _refresh_media_playlist(session, mediaItem);
        if (_finish_download_last(session, mediaItem) > 0) {
            if (mediaItem->media_err_code < 0) {
                break;
            }
            LOGI("[Type : %d] download all segments,worker sleep...", mediaItem->media_type);
            mediaItem->media_eof_flag = 1;
            mediaItem->media_err_code = 0;
            _thread_wait_timeUs(session, mediaItem, -1);
            mediaItem->media_monitor_timer = get_clock_monotonic_us();
        }
        while (session->is_closed < 1 && session->interrupt && (*session->interrupt)()) {
            if (mediaItem->media_handling_seek > 0) {
                break;
            }
            amthreadpool_thread_usleep(100 * 1000); //waked by close or read.
        }
        while (mediaItem->media_handling_seek > 0) {
            if (mediaItem->media_seek_flag == 2) {
                pass = 1;
            }
            if (mediaItem->media_seek_flag > 0) {
                mediaItem->media_seek_flag = 0;
            }
            amthreadpool_thread_usleep(10 * 1000);
        }
    } while (session->is_closed < 1);
    if (mediaItem->media_err_code != 0) {
        mediaItem->media_err_code = -(DOWNLOAD_EXIT_CODE);
    }
    LOGI("[Type : %d] download worker end,error code:%d", mediaItem->media_type, mediaItem->media_err_code);
    session->is_closed = 1; // quit all of download.
    return (void*)NULL;
}

static int _open_session_download_task(M3ULiveSession* s)
{
    pthread_t tid;
    int ret = -1;
    pthread_attr_t pthread_attr;

    if (s->is_mediagroup <= 0) {
        pthread_attr_init(&pthread_attr);

        ret = hls_task_create(&tid, &pthread_attr, _download_worker, s);
        pthread_setname_np(tid, "hls_m3ulivesession");
        if (ret != 0) {
            pthread_attr_destroy(&pthread_attr);
            return -1;
        }

        pthread_attr_destroy(&pthread_attr);
        s->tid = tid;
    } else {
        int i;
        for (i = 0; i < s->media_item_num; i++) {
            pthread_attr_init(&pthread_attr);
            ret = hls_task_create(&tid, &pthread_attr, _media_download_worker, s->media_item_array[i]);
            pthread_setname_np(tid, "media downloader");
            if (ret != 0) {
                LOGE("Failed create media downloader !");
                pthread_attr_destroy(&pthread_attr);
                return -1;
            }
            pthread_attr_destroy(&pthread_attr);
            s->media_item_array[i]->media_tid = tid;
        }
    }

    LOGV("Open live session download task\n");
    return 0;

}

static void _pre_estimate_bandwidth(M3ULiveSession* s)
{
    if (s == NULL) {
        return;
    }

    int ret = -1;
    if (s->is_variant > 0) {
        M3uBaseNode* node = NULL;
        node = m3u_get_node_by_index(s->playlist, 0);
        if (node) {
            void *handle = NULL;
            char headers[MAX_URL_SIZE] = {0};
            if (s->headers != NULL) {
                strncpy(headers, s->headers, MAX_URL_SIZE);
            }
            if (s->cookies && strlen(s->cookies) > 0) {
                if (s->headers != NULL && strlen(s->headers) > 0 && s->headers[strlen(s->headers) - 1] != '\n') {
                    snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", s->cookies);
                } else {
                    snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", s->cookies);
                }
            }

            if (s->is_encrypt_media > 0) {
                AESKeyInfo_t keyinfo;
                int indexInPlaylist = node->index;
                ret = _get_decrypt_key(s, NULL, indexInPlaylist, &keyinfo);
                if (ret != 0) {
                    return;
                }
                ret = hls_http_open(node->fileUrl, headers, (void*)&keyinfo, &handle);
                if (keyinfo.key_info != NULL) {
                    free(keyinfo.key_info);
                }
            } else {
                ret = hls_http_open(node->fileUrl, headers, NULL, &handle);
            }
            if (ret != 0) {
                hls_http_close(handle);
                handle = NULL;
                return;
            }
            int pre_bw = preEstimateBandwidth(handle, NULL, 0);
            size_t index = s->bandwidth_item_num - 1;
            while (index > 0 && (s->bandwidth_list[index]->mBandwidth > (size_t)pre_bw)) {
                --index;
            }
            LOGV("preEstimateBandwidth, bw=%f kbps, index=%d", pre_bw / 1024.0f, index);
            char* url = NULL;
            if (s->bandwidth_item_num > 0) {
                url = s->bandwidth_list[index]->url;
            } else {
                LOGE("Never get bandwidth list\n");
                return;
            }

            int unchanged = 0;
            void* playlist = _fetch_play_list(url, s, NULL, &unchanged, index);
            if (playlist == NULL) {
                if (unchanged) {
                    LOGE("Never see this line\n");
                } else {
                    LOGE("[%s:%d] failed to load playlist at url '%s'", __FUNCTION__, __LINE__, url);
                    return;
                }
            } else {
                if (s->durationUs < 1 && s->bandwidth_list[index]->playlist != NULL) { //live
                    m3u_release(s->bandwidth_list[index]->playlist);
                }
                s->bandwidth_list[index]->playlist = playlist;
                s->prev_bandwidth_index = index;
                s->playlist = playlist;
            }

        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

static SessionMediaItem * _init_session_mediaItem(M3ULiveSession * ss, MediaType type, const char * groupID, const char * url) {
    M3uMediaItem * item = NULL;
    if (groupID) {
        item = m3u_get_media_by_groupID(ss->master_playlist, type, groupID);
        if (item == NULL) {
            LOGE("[%s:%d] Fail to get media item !", __FUNCTION__, __LINE__);
            return NULL;
        }
    }
    SessionMediaItem * mediaItem = (SessionMediaItem *)malloc(sizeof(SessionMediaItem));
    mediaItem->media_type = type;
    if (item) {
        mediaItem->media_url = strdup(item->mediaUrl);
    } else {
        mediaItem->media_url = strdup(url);
    }
    if (ss->cookies) {
        mediaItem->media_cookies = strdup(ss->cookies);
    }
    mediaItem->media_redirect = NULL;
    mediaItem->media_last_m3u8_url = NULL;
    mediaItem->media_last_segment_url = NULL;
    mediaItem->media_cookies = NULL;
    memset(mediaItem->media_last_bandwidth_list_hash, 0, sizeof(mediaItem->media_last_bandwidth_list_hash));
    mediaItem->media_playlist = NULL;
    mediaItem->media_cache = NULL;
    mediaItem->session = (void *)ss;
    mediaItem->media_cur_seq_num = -1;
    mediaItem->media_last_fetch_timeUs = -1;
    mediaItem->media_seek_timeUs = -1;
    mediaItem->media_switch_anchor_timeUs = -1;
    mediaItem->media_first_seq_num = -1;
    mediaItem->media_cur_bandwidth_index = -1;
    mediaItem->media_estimate_bandwidth_bps = 0;
    mediaItem->media_estimate_bps = 0;
    mediaItem->media_refresh_state = INITIAL_MINIMUM_RELOAD_DELAY;
    mediaItem->media_retries_num = 0;
    mediaItem->media_err_code = 0;
    mediaItem->media_eof_flag = 0;
    mediaItem->media_seek_flag = -1;
    mediaItem->media_handling_seek = 0;
    mediaItem->media_no_new_file = 0;
    mediaItem->media_codec_buffer_time_s = 0;
    mediaItem->media_sub_ready = 0;
    mediaItem->media_encrypted = -1;
    mediaItem->media_aes_keyurl_list_num = 0;
    INIT_LIST_HEAD(&mediaItem->media_aes_key_list);
    pthread_mutex_init(&mediaItem->media_lock, NULL);
    pthread_cond_init(&mediaItem->media_cond, NULL);
    mediaItem->media_dump_handle = NULL;
    if (ss->media_dump_mode > 0 && (type == TYPE_AUDIO || type == TYPE_VIDEO)) {
        char dump_path[MAX_URL_SIZE] = {0};
        snprintf(dump_path, MAX_URL_SIZE, "/data/tmp/%s_read_dump.dat", type == TYPE_AUDIO ? "audio" : "video");
        mediaItem->media_dump_handle = fopen(dump_path, "ab+");
    }
    return mediaItem;
}

static int _reinit_session_mediaItem(M3ULiveSession * ss, SessionMediaItem * mediaItem, int bandwidth_index) {
    BandwidthItem_t * bandItem = ss->bandwidth_list[bandwidth_index];
    if (mediaItem->media_url) {
        free(mediaItem->media_url);
    }
    if (mediaItem->media_redirect) {
        free(mediaItem->media_redirect);
    }
    M3uMediaItem * item = NULL;
    if ((mediaItem->media_type & TYPE_AUDIO) && bandItem->node->audio_groupID[0] != '\0') {
        item = m3u_get_media_by_groupID(ss->master_playlist, mediaItem->media_type, bandItem->node->audio_groupID);
        if (!item) {
            LOGE("[%s:%d] Fail to get media item !", __FUNCTION__, __LINE__);
            goto FAIL;
        }
    } else if ((mediaItem->media_type & TYPE_VIDEO) && bandItem->node->video_groupID[0] != '\0') {
        item = m3u_get_media_by_groupID(ss->master_playlist, mediaItem->media_type, bandItem->node->video_groupID);
        if (!item) {
            LOGE("[%s:%d] Fail to get media item !", __FUNCTION__, __LINE__);
            goto FAIL;
        }
    } else if ((mediaItem->media_type & TYPE_SUBS) && bandItem->node->sub_groupID[0] != '\0') {
        item = m3u_get_media_by_groupID(ss->master_playlist, mediaItem->media_type, bandItem->node->sub_groupID);
        if (!item) {
            LOGE("[%s:%d] Fail to get media item !", __FUNCTION__, __LINE__);
            goto FAIL;
        }
    } else {
        mediaItem->media_url = strdup(bandItem->node->fileUrl);
        return 0;
    }
    mediaItem->media_url = strdup(item->mediaUrl);
    return 0;

FAIL:
    return -1;
}

static void _release_bandwidth_and_media_item(M3ULiveSession * session) {
    int i = 0;

    if (session->bandwidth_item_num > 0) {
        for (i = 0; i < session->bandwidth_item_num; i++) {
            BandwidthItem_t* item = session->bandwidth_list[i];
            if (item) {
                if (item->url != NULL) {
                    if (session->log_level >= HLS_SHOW_URL) {
                        LOGV("Release bandwidth list,index:%d,url:%s\n", i, item->url);
                    } else {
                        LOGV("Release bandwidth list,index:%d\n", i);
                    }
                    free(item->url);
                }
                if (item->playlist != NULL) {
                    m3u_release(item->playlist);
                }
                if (item->redirect != NULL) {
                    free(item->redirect);
                }
                free(item);
            }
        }
        in_freepointer(&session->bandwidth_list);
        session->bandwidth_item_num = 0;
        session->playlist = NULL;
        if (session->master_playlist) {
            m3u_release(session->master_playlist);
        }
    }

    for (i = 0; i < session->media_item_num; i++) {
        SessionMediaItem * mediaItem = session->media_item_array[i];
        if (mediaItem->media_url) {
            free(mediaItem->media_url);
        }
        if (mediaItem->media_redirect) {
            free(mediaItem->media_redirect);
        }
        if (mediaItem->media_last_m3u8_url) {
            free(mediaItem->media_last_m3u8_url);
        }
        if (mediaItem->media_last_segment_url) {
            free(mediaItem->media_last_segment_url);
        }
        if (mediaItem->media_cookies) {
            free(mediaItem->media_cookies);
        }
        if (mediaItem->media_playlist) {
            m3u_release(mediaItem->media_playlist);
        }
        if (mediaItem->media_cache) {
            hls_simple_cache_free(mediaItem->media_cache);
        }
        if (mediaItem->media_aes_keyurl_list_num > 0) {
            AESKeyForUrl_t * pos = NULL;
            AESKeyForUrl_t * tmp = NULL;
            list_for_each_entry_safe(pos, tmp, &mediaItem->media_aes_key_list, key_head) {
                list_del(&pos->key_head);
                free(pos);
                pos = NULL;
                mediaItem->media_aes_keyurl_list_num--;
            }
        }
        if (mediaItem->media_dump_handle) {
            fclose(mediaItem->media_dump_handle);
        }
        pthread_mutex_destroy(&mediaItem->media_lock);
        pthread_cond_destroy(&mediaItem->media_cond);
        free(mediaItem);
        mediaItem = NULL;
        session->media_item_num--;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

//========================================API============================================

int m3u_session_open(const char* baseUrl, const char* headers, void** hSession, void *urlcontext)
{
    if (baseUrl == NULL || strlen(baseUrl) < 2) {
        LOGE("Check input baseUrl\n");
        *hSession = NULL;
        return -1;
    }
    int ret = -1;
    char *str;
    M3ULiveSession* session = (M3ULiveSession*)malloc(sizeof(M3ULiveSession));
    _init_m3u_live_session_context(session);

    int dumy = 0;

    void* base_list = NULL;
    if (baseUrl[0] == 's') {
        session->baseUrl = strdup(baseUrl + 1);
    } else {
        session->baseUrl = strdup(baseUrl);
    }
    if (headers != NULL && strlen(headers) > 0) {
        LOGI("[%s:%d]old header =[%s] \n", __FUNCTION__, __LINE__, headers);
        char *cookies_start = strstr(headers, "Cookie: ");
        if (cookies_start) {
            int endchar_num = 2;
            session->headers = malloc(MAX_URL_SIZE);
            session->cookies = malloc(MAX_URL_SIZE);
            memset(session->headers, 0, MAX_URL_SIZE);
            memset(session->cookies, 0, MAX_URL_SIZE);

            // copy the front of cookies
            if (cookies_start > headers) {
                snprintf(session->headers, (cookies_start - headers) + 2, "%s", headers);
            }

            // copy cookies to local and copy the afterward
            char *cookies_end = strstr(cookies_start, "\r\n");
            if (!cookies_end) {
                cookies_end = strstr(cookies_start, "\n");
                endchar_num = 1;
            }

            if (cookies_end) {
                strncpy(session->cookies, cookies_start + 8, (cookies_end - cookies_start) - 8);

                if (session->headers != NULL && strlen(session->headers) > 0) {
                    snprintf(session->headers + strlen(session->headers), MAX_URL_SIZE - strlen(session->headers), "\r\n%s", cookies_end + endchar_num);
                } else {
                    snprintf(session->headers + strlen(session->headers), MAX_URL_SIZE - strlen(session->headers), "%s", cookies_end + endchar_num);
                }
            } else {
                strncpy(session->cookies, cookies_start, MAX_URL_SIZE);
            }
        } else {
            session->headers = strdup(headers);
        }
        if (session->headers && session->cookies) {
            LOGI("[%s:%d]header=[%s][%d] cookies=[%s][%d]\n", __FUNCTION__, __LINE__, session->headers, strlen(session->headers), session->cookies, strlen(session->cookies));
        }
    }

    char* tmp = strstr(baseUrl, "livemode=1");
    if (tmp) {
        session->is_livemode = 1;
    }

    if (session->log_level >= HLS_SHOW_URL) {
        LOGI("Open baseUrl :%s\n", session->baseUrl);
    }
    if ((str = strstr(session->baseUrl, "GD_LIVESEEK=")) != NULL && str != session->baseUrl) { /*has ext gd_seek info.*/
        session->ext_gd_seek_info = strdup(str + strlen("GD_LIVESEEK="));
        LOGI("found GD_LIVESEEK flags from baseUrl %s\n", session->baseUrl);
        str[-1] = '\0'; /*del session->baseUrl's "?GD_LIVESEEK" */
        LOGI("get GD_LIVESEEK %s\n", session->ext_gd_seek_info);
        LOGI("Changed Base Url %s\n", session->baseUrl);
    }
    session->urlcontext = urlcontext;

#ifdef USE_SIMPLE_CACHE
    int cache_size_max = 1024 * 1024 * 10; //10M
    if (am_getconfig_bool_def("media.amplayer.low_ram", 0)) {
        cache_size_max = cache_size_max / 4;
    }
    ret = hls_simple_cache_alloc(cache_size_max, &session->cache);
    if (ret != 0) {
        ERROR_MSG();
        *hSession = session;
        goto fail_open;
    }
#endif

    base_list = _fetch_play_list(session->baseUrl, session, NULL, &dumy, -1);

    if (base_list == NULL) {
        ERROR_MSG();
        *hSession = session;
        ret = -1;
        goto fail_open;
    }
    session->master_playlist = base_list;
    session->is_mediagroup = m3u_get_mediaGroup_num(base_list) > 0;
    if (m3u_is_variant_playlist(base_list) > 0) { //add to bandwidth list
        int i = 0;
        int node_num = m3u_get_node_num(base_list);
        int filter_threshold = AUDIO_BANDWIDTH_MAX;
        float value = 0.0;
        if (!am_getconfig_float("libplayer.hls.bwthreshold", &value)) {
            filter_threshold = (int)value;
        }
        session->is_except = strcmp(EXCEPSOURCE, session->baseUrl);
        for (i = 0; i < node_num; i++) {
            M3uBaseNode* node = m3u_get_node_by_index(base_list, i);
            if (node == NULL) {
                LOGE("Failed to get node\n");
                m3u_release(base_list);
                session->bandwidth_list = NULL;
                session->playlist = NULL;
                session->master_playlist = NULL;
                *hSession = session;
                ret = -1;
                goto fail_open;
            }
            if (node->bandwidth > filter_threshold) {
                break;
            }
        }
        if (i == node_num) { // all audio streams.
            filter_threshold = 0;
        }
        for (i = 0; i < node_num; i++) {

            M3uBaseNode* node = m3u_get_node_by_index(base_list, i);

#if 1
            if (node->bandwidth > 0 && ((node->bandwidth < filter_threshold && node->bandwidth > BANDWIDTH_THRESHOLD)
                                        || (node->bandwidth < filter_threshold / 1000))) {
                if (session->log_level >= HLS_SHOW_URL) {
                    LOGV("This variant can't playback,drop it,url:%s,bandwidth:%d\n", node->fileUrl, node->bandwidth);
                } else {
                    LOGV("This variant can't playback,drop it,bandwidth:%d\n", node->bandwidth);
                }
                continue;
            }
            if (!session->is_except && !i) {
                continue;
            }
#endif

            BandwidthItem_t* item = (BandwidthItem_t*)malloc(sizeof(BandwidthItem_t));

            memset(item, 0, sizeof(BandwidthItem_t));
            if (!session->ext_gd_seek_info) {
                item->url = strdup(node->fileUrl);
            } else {
                item->url = malloc(strlen(node->fileUrl) + strlen(session->ext_gd_seek_info) + 2);
                strcpy(item->url, node->fileUrl);
                //item->url = strndup(node->fileUrl, strlen(node->fileUrl)+strlen(session->ext_gd_seek_info)+2);
                char * ext_str = get_ext_gd_seek_info(session->ext_gd_seek_info);
                if (ext_str != NULL) {
                    strcat(item->url, "?playseek=");
                    strcat(item->url, ext_str);
                    free(ext_str);
                } else {
                    strcat(item->url, session->ext_gd_seek_info);
                }
                LOGV("add variant ext info(%p) %s, len: %d\n", item->url, item->url, strlen(item->url));
            }

            item->mBandwidth = node->bandwidth;
            item->program_id = node->program_id;
            item->playlist = NULL;
            item->redirect = NULL;
            item->node = node;

            if (!session->stbId_string) {
                session->stbId_string = _get_stbid_string(item->url);
            }

            in_dynarray_add(&session->bandwidth_list, &session->bandwidth_item_num, item);

            LOGV("add item to session,num:%d\n", session->bandwidth_item_num);

        }

        //sort all bandwidths
        _sort_m3u_session_bandwidth(session);
        session->playlist = NULL;
        session->is_opened = 1;
        session->is_variant = 1;
    } else {
        session->playlist = base_list;
        session->is_opened = 1;
        if (!session->stbId_string) {
            session->stbId_string = _get_stbid_string(baseUrl);
        }
    }

    ret = _choose_bandwidth_and_init_playlist(session);
    if (ret < 0) {
        ERROR_MSG();
        *hSession = session;
        goto fail_open;
    }

    if (in_get_sys_prop_bool("media.libplayer.hlsestbw") > 0 && session->is_mediagroup <= 0) {
        _pre_estimate_bandwidth(session);
    }

    ret = _open_session_download_task(session);

    session->bw_meausure_handle = bandwidth_measure_alloc(BW_MEASURE_ITEM_DEFAULT, 0);

    LOGI("Session open complete\n");
    *hSession = session;
    return 0;

fail_open:
    LOGE("failed to open Session %x\n", *hSession);
    if (*hSession != NULL) {
        m3u_session_close(*hSession);
    }
    *hSession = NULL;
    return ret;
}

int m3u_session_is_seekable(void* hSession)
{
    int seekable = 0;
    if (hSession == NULL) {
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    pthread_mutex_lock(&session->session_lock);
    if (session->durationUs > 0) {
        seekable = 1;
    }
    pthread_mutex_unlock(&session->session_lock);

    return seekable;
}
int m3u_session_get_livemode(void* hSession, int *pnLivemode)
{
    if (NULL == hSession) {
        LOGE("[%s:%d]Invalid session\n", __FUNCTION__, __LINE__);
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;

    *pnLivemode = session->is_livemode;
    return 0;
}

int64_t m3u_session_seekUs(void* hSession, int64_t posUs, int (*interupt_func_cb)())
{
    int seekable = 0;
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;
    int64_t realPosUs = posUs;
    M3uBaseNode* node = NULL;
    if (session->is_mediagroup > 0) {
        int i;
        for (i = 0; i < session->media_item_num; i++) {
            session->media_item_array[i]->media_handling_seek = 1;
            amthreadpool_pool_thread_cancel(session->media_item_array[i]->media_tid);
            pthread_mutex_lock(&session->media_item_array[i]->media_lock);
            pthread_cond_broadcast(&session->media_item_array[i]->media_cond);
            session->media_item_array[i]->media_seek_flag = 1;
            session->media_item_array[i]->media_eof_flag = 0;
            session->media_item_array[i]->media_seek_timeUs = posUs;
        }
        if (session->media_item_array[0]->media_playlist != NULL) {
            node = m3u_get_node_by_time(session->media_item_array[0]->media_playlist, posUs);
            if (node != NULL) {
                realPosUs = node->startUs;
            }
        }
        session->seekflag = 1;
        LOGI("[%s:%d] posUs=%lld, realPosUs=%lld", __FUNCTION__, __LINE__, posUs, realPosUs);
        for (i = 0; i < session->media_item_num; i++) {
            pthread_mutex_unlock(&session->media_item_array[i]->media_lock);
        }
        int media_count = 0;
        while (media_count < session->media_item_num) {
            media_count = 0;
            if (interupt_func_cb != NULL && interupt_func_cb() > 0) {
                break;
            }
            for (i = 0; i < session->media_item_num; i++) {
                if (session->media_item_array[i]->media_seek_flag <= 0) {
                    media_count++;
                }
            }
            amthreadpool_thread_usleep(1000 * 10);
        }
        session->seekflag = 0;
        for (i = 0; i < session->media_item_num; i++) {
            amthreadpool_thread_uncancel(session->media_item_array[i]->media_tid);
#ifdef USE_SIMPLE_CACHE
            pthread_mutex_lock(&session->media_item_array[i]->media_lock);
            hls_simple_cache_reset(session->media_item_array[i]->media_cache);
            pthread_mutex_unlock(&session->media_item_array[i]->media_lock);
#endif
            session->media_item_array[i]->media_handling_seek = 0;
        }
        return realPosUs;
    } else {
        session->handling_seek = 1;
        amthreadpool_pool_thread_cancel(session->tid);
        pthread_mutex_lock(&session->session_lock);
        if (session->playlist != NULL) {
            node = m3u_get_node_by_time(session->playlist, posUs);
            if (node != NULL) {
                realPosUs = node->startUs;
            }
        }
        pthread_cond_broadcast(&session->session_cond);
        session->seekflag = 1;
        session->seektimeUs = posUs;
        session->eof_flag = 0;
        if (node != NULL) {
            session->startsegment_index = node->index;
        }
        session->output_stream_offset = 0;

        if (session->is_livemode == 1 && session->need_refresh_playlist <= 0
            && posUs < 61000000) { // use 1 sec as step size when live to timeshift.
            posUs = 61000000;
            realPosUs = 61000000;
            session->seektimeUs = posUs;
        }

        if (session->is_livemode == 1 && session->need_refresh_playlist > 0 && posUs < 10000000) {
            realPosUs = 0;
            posUs = 0;
            session->seektimeUs = posUs;
            LOGI("[%s:%d]short timeswitch request to revert to live request\n", __FUNCTION__, __LINE__);
        }

        if (session->is_livemode == 1) {
            if (posUs > 0) {
                session->need_refresh_playlist = 1;
            } else {
                session->need_refresh_playlist = 0;
                session->timeshift_url_switch = 0;
                session->last_timepoint = 0;
            }
            session->last_bandwidth_list_fetch_timeUs = -1;
            session->refresh_point = posUs;
            session->timeshift_force_refresh = 0;
        }

        LOGI("[%s:%d]startsegment_index=%d,posUs=%lld,realPosUs=%lld\n", __FUNCTION__, __LINE__, session->startsegment_index, posUs, realPosUs);
        pthread_mutex_unlock(&session->session_lock);

        while (session->seekflag == 1) { //ugly codes,just block app
            if (interupt_func_cb != NULL) {
                if (interupt_func_cb() > 0) {
                    break;
                }
            }
            amthreadpool_thread_usleep(1000 * 10);
        }
        amthreadpool_thread_uncancel(session->tid);
#ifdef USE_SIMPLE_CACHE
        pthread_mutex_lock(&session->session_lock);
        hls_simple_cache_reset(session->cache);
        pthread_mutex_unlock(&session->session_lock);
#endif
        session->handling_seek = 0;
        if ((posUs - realPosUs) > POS_SEEK_THRESHOLD) {
            return posUs - POS_SEEK_THRESHOLD;
        } else {
            return realPosUs;
        }
    }
}
int64_t m3u_session_seekUs_offset(void* hSession, int64_t posUs, int64_t *streamoffset)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;
    if (session->is_livemode == 1 || session->is_mediagroup > 0) {
        LOGI("[%s:%d]live mode can't do loopbuffer seek. posUs=%lld\n", __FUNCTION__, __LINE__, posUs);
        return -1;
    }

    LOGI("[%s:%d]Doing loopbuffer offset seek posUs=%lld\n", __FUNCTION__, __LINE__, posUs);
    int cur_index = 0, seek_index = 0;
    int firstSeqNumberInPlaylist = 0;
    if (session->playlist == NULL) {
        return -1;
    }

    firstSeqNumberInPlaylist = m3u_get_node_by_index(session->playlist, 0)->media_sequence;
    cur_index = session->cur_seq_num - firstSeqNumberInPlaylist;                            // hls current download segment index

    M3uBaseNode* node = m3u_get_node_by_time(session->playlist, posUs);
    if (node == NULL) {
        LOGE("[%s:%d]can't find posUs=%lld", posUs);
        return -1;
    }
    seek_index = node->index;                                                   // seek segment index

    if (seek_index >= cur_index || seek_index < session->startsegment_index) {
        LOGE("[%s:%d]seek out of range posUs=%lld,seek=%d,cur=%d,start=%d\n", __FUNCTION__, __LINE__,
             posUs, seek_index, cur_index, session->startsegment_index);
        return -1;
    }

    int64_t ret = m3u_get_node_span_size(session->playlist, session->startsegment_index, seek_index);
    if (ret < 0) {
        LOGE("[%s:%d]get span failed posUs=%lld,seek=%d,cur=%d,start=%d\n", __FUNCTION__, __LINE__,
             posUs, seek_index, cur_index, session->startsegment_index);
        return -1;
    }
    *streamoffset = session->output_stream_offset - ret;

    LOGI("[%s:%d]posUs=%lld,startUs=%lld; seek=%d,cur=%d,start=%d; streamoffset=%lld,output_stream=%lld,ret=%lld\n", __FUNCTION__, __LINE__,
         posUs, node->startUs, seek_index, cur_index, session->startsegment_index, *streamoffset, session->output_stream_offset, ret);
    return node->startUs;
}
int m3u_session_get_stream_num(void* hSession, int* num)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;

    int stream_num = 0;

    pthread_mutex_lock(&session->session_lock);

    stream_num = session->bandwidth_item_num;

    pthread_mutex_unlock(&session->session_lock);

    *num = stream_num;
    return 0;

}

int m3u_session_get_durationUs(void*hSession, int64_t* dur)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;

    int64_t duration = 0;

    pthread_mutex_lock(&session->session_lock);

    duration = session->durationUs;
    if (session->is_livemode == 1) {
        duration = 1;
    }

    pthread_mutex_unlock(&session->session_lock);

    *dur = duration;
    return 0;

}
int m3u_session_get_cur_bandwidth(void* hSession, int* bw)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;

    int bandwidth = 0;

    if (session->bandwidth_item_num > 0) {
        if (session->bandwidth_item_num == 1 && session->bandwidth_list != NULL) {
            bandwidth = session->bandwidth_list[0]->mBandwidth;
        } else {

            bandwidth = session->bandwidth_list[session->prev_bandwidth_index]->mBandwidth;
        }

    } else if (session->stream_estimate_bps > 0) {
        bandwidth = session->stream_estimate_bps;
        LOGV("Got current stream estimate bandwidth,%d\n", bandwidth);
    }

    *bw = bandwidth;
    return 0;
}

int m3u_session_get_cached_data_time(void*hSession, int* time)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;
    *time = session->cached_data_timeUs / 1000000;
    return 0;
}

int m3u_session_get_estimate_bps(void*hSession, int* bps)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;
    *bps = session->stream_estimate_bps;
    return 0;

}

int m3u_session_get_estimate_bandwidth(void*hSession, int* bps)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;
    *bps = session->estimate_bandwidth_bps;
    return 0;

}

int m3u_session_get_error_code(void*hSession, int* errcode)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3ULiveSession* session = (M3ULiveSession*)hSession;
    *errcode = session->err_code;
    return 0;
}
int m3u_session_set_codec_data(void* hSession, int time)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    // TODO: add media group codec data time here
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    session->codec_data_time = time;

    return 0;
}

int m3u_session_read_data(void* hSession, void* buf, int len)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    int ret = 0;
    M3ULiveSession* session = (M3ULiveSession*)hSession;
#ifdef USE_SIMPLE_CACHE
    ret = hls_simple_cache_block_read(session->cache, buf, len, 100 * 1000);
#endif
    if (ret == 0) {
        if (session->err_code < 0) {
            return session->err_code;
        }
        if (session->eof_flag == 1) {
            return 0;
        }
        return HLSERROR(EAGAIN);
    }

    if (ret > 0) {
        session->output_stream_offset += ret;
    }

    return ret;
}

int m3u_session_register_interrupt(void* hSession, int (*interupt_func_cb)())
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;
    session->interrupt = interupt_func_cb;
    return 0;
}

int m3u_session_close(void* hSession)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    LOGI("Receive close command\n");
    session->is_closed = 1;
    if (session->is_mediagroup > 0) {
        int i;
        for (i = 0; i < session->media_item_num; i++) {
            amthreadpool_thread_wake(session->media_item_array[i]->media_tid);
            if (session->media_item_array[i]->media_tid != 0) {
                LOGV("[Type : %d] Terminate session download task", session->media_item_array[i]->media_type);
                _thread_wake_up(session, session->media_item_array[i]);
                hls_task_join(session->media_item_array[i]->media_tid, NULL);
            }
        }
    } else {
        amthreadpool_thread_wake(session->tid);
        if (session->tid != 0) {
            LOGV("Terminate session download task\n");
            _thread_wake_up(session, NULL);
            hls_task_join(session->tid, NULL);
        }
    }
    if (session->cookies) {
        free(session->cookies);
    }
    if (session->baseUrl) {
        free(session->baseUrl);
    }
    if (session->headers) {
        free(session->headers);
    }
    if (session->redirectUrl) {
        free(session->redirectUrl);
    }
    if (session->last_m3u8_url) {
        free(session->last_m3u8_url);
    }
#ifdef LIVEPLAY_SEEK
    if (session->last_timeshift_segment_url) {
        free(session->last_timeshift_segment_url);
    }
#endif
    if (session->ext_gd_seek_info) {
        free(session->ext_gd_seek_info);
    }
    if (session->last_segment_url) {
        free(session->last_segment_url);
    }
    if (session->stbId_string) {
        free(session->stbId_string);
    }
    _release_bandwidth_and_media_item(session);
    if (session->aes_keyurl_list_num > 0) {
        int j = 0;
        for (j = 0; j < session->aes_keyurl_list_num; j++) {
            AESKeyForUrl_t* akey = session->aes_keyurl_list[j];
            if (akey) {
                free(akey);
            }
        }
        in_freepointer(&session->aes_keyurl_list);
        session->aes_keyurl_list_num = 0;
    }
    if (session->playlist) {
        m3u_release(session->playlist);
    }
    if (session->bw_meausure_handle != NULL) {
        bandwidth_measure_free(session->bw_meausure_handle);
    }
#ifdef USE_SIMPLE_CACHE
    if (session->cache != NULL) {
        hls_simple_cache_free(session->cache);
    }
#endif
    pthread_mutex_destroy(&session->session_lock);
    pthread_cond_destroy(&session->session_cond);
    free(session);
    LOGI("m3u live session released\n");

    return 0;

}
//==================================================================
//==ugly codes for cmf&by peter,20130424

int64_t m3u_session_get_next_segment_st(void* hSession)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        return -1;
    }
    int firstSeqInPlaylist = 0;
    M3uBaseNode* item = m3u_get_node_by_index(session->playlist, 0);
    if (item->media_sequence > 0) {
        firstSeqInPlaylist = item->media_sequence;
    }
    int lastSeqInPlaylist = firstSeqInPlaylist + m3u_get_node_num(session->playlist) - 1;
    if (session->cur_seq_num == lastSeqInPlaylist) {
        return session->durationUs;
    }
    int next_index = session->cur_seq_num - firstSeqInPlaylist + 1;
    item = m3u_get_node_by_index(session->playlist, next_index);
    return item->startUs;
}
int m3u_session_get_segment_num(void* hSession)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        ERROR_MSG();
        return -1;
    }
    return m3u_get_node_num(session->playlist);
}
int64_t m3u_session_hybrid_seek(void* hSession, int64_t seg_st, int64_t pos, int (*interupt_func_cb)())
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    int64_t realPosUs = seg_st;
    pthread_mutex_lock(&session->session_lock);
    pthread_cond_broadcast(&session->session_cond);
    session->seekflag = 2;
    session->seektimeUs = seg_st;
    session->seekposByte = pos;
    session->eof_flag = 0;
    pthread_mutex_unlock(&session->session_lock);

    while (session->seekflag == 2) { //ugly codes,just block app
        if (interupt_func_cb != NULL) {
            if (interupt_func_cb() > 0) {
                break;
            }
        }
        amthreadpool_thread_usleep(1000 * 10);
    }
    return seg_st;

}
void* m3u_session_seek_by_index(void* hSession, int prev_index, int index, int (*interupt_func_cb)()) //block api
{
    if (hSession == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3uBaseNode* item  = m3u_get_node_by_index(session->playlist, index);

    int64_t realPosUs = item->startUs;
    pthread_mutex_lock(&session->session_lock);
    pthread_cond_broadcast(&session->session_cond);
    session->seekflag = 2;
    session->seektimeUs = realPosUs;
    session->eof_flag = 0;
    pthread_mutex_unlock(&session->session_lock);


    while (session->seekflag == 2) {
        amthreadpool_thread_usleep(1000 * 10);
        if (interupt_func_cb != NULL) {
            if (interupt_func_cb() > 0) {
                TRACE();
                break;
            }
        }
    }
    return item;
}
int64_t m3u_session_get_segment_size(void* hSession, const char* url, int index, int type)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        ERROR_MSG();
        return -1;
    }

    M3uBaseNode* item  = m3u_get_node_by_index(session->playlist, index);
    if (type == 1) {
        LOGV("Get segment size:%lld\n", item->range_length);
        return item->range_length;
    } else if (type == 2) {
        void* handle = NULL;
        char headers[MAX_URL_SIZE] = {0};
        if (session->headers != NULL) {
            strncpy(headers, session->headers, MAX_URL_SIZE);
        }
        if (session->cookies && strlen(session->cookies) > 0) {
            if (session->headers != NULL && strlen(session->headers) > 0 && session->headers[strlen(session->headers) - 1] != '\n') {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "\r\nCookie: %s\r\n", session->cookies);
            } else {
                snprintf(headers + strlen(headers), MAX_URL_SIZE - strlen(headers), "Cookie: %s\r\n", session->cookies);
            }
        }
        int rv = hls_http_open(url, headers, NULL, &handle);
        if (rv != 0) {
            if (handle != NULL) {
                hls_http_close(handle);
            }
            return -1;
        }
        int64_t len = hls_http_get_fsize(handle);
        if (handle != NULL) {
            hls_http_close(handle);
        }
        return len;

    } else if (type == 3) {
        if (session->stream_estimate_bps > 0) {
            int64_t len = (session->stream_estimate_bps * item->durationUs) / (8 * 1000000);
            return len;
        } else {
            return 0;
        }
    }
    return 0;

}
void* m3u_session_get_index_by_timeUs(void* hSession, int64_t timeUs)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        ERROR_MSG();
        return NULL;
    }

    M3uBaseNode* item  = m3u_get_node_by_time(session->playlist, timeUs);
    if (item == NULL) {
        ERROR_MSG();
        return NULL;
    }
    return item;
}
void* m3u_session_get_segment_info_by_index(void* hSession, int index)
{
    if (hSession == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3ULiveSession* session = (M3ULiveSession*)hSession;

    if (session->playlist == NULL) {
        ERROR_MSG();
        return NULL;
    }

    M3uBaseNode* item  = m3u_get_node_by_index(session->playlist, index);
    return item;
}

//////////////////////////////////// media group api //////////////////////////////////////
int m3u_session_media_read_data(void * hSession, int stream_index, uint8_t * buf, int len) {
    if (hSession == NULL || stream_index > MEDIA_TYPE_NUM - 1) {
        ERROR_MSG();
        return -1;
    }
    int ret = 0;
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    SessionMediaItem * mediaItem = session->media_item_array[stream_index];
#ifdef USE_SIMPLE_CACHE
    void * cache = mediaItem->media_cache;
    ret = hls_simple_cache_read(cache, buf, len); // noblock
#endif
    if (mediaItem->media_dump_handle && ret > 0) {
        fwrite(buf, 1, ret, mediaItem->media_dump_handle);
        fflush(mediaItem->media_dump_handle);
    }
    if (!ret) {
        if (mediaItem->media_err_code < 0) {
            return mediaItem->media_err_code;
        }
        if (mediaItem->media_eof_flag == 1) {
            return HLS_STREAM_EOF;
        }
        return HLSERROR(EAGAIN);
    }

    return ret;
}

int m3u_session_media_get_current_bandwidth(void * hSession, int stream_index, int * bw) {
    if (hSession == NULL || stream_index > MEDIA_TYPE_NUM - 1) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    *bw = session->media_item_array[stream_index]->media_estimate_bps;
    return 0;
}

int m3u_session_media_set_codec_buffer_time(void * hSession, int stream_index, int buffer_time_s) {
    if (hSession == NULL || stream_index > MEDIA_TYPE_NUM - 1) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    session->media_item_array[stream_index]->media_codec_buffer_time_s = buffer_time_s;
    return 0;
}

int m3u_session_media_get_track_count(void * hSession) {
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    return m3u_get_track_count(session->master_playlist);
}

M3uTrackInfo * m3u_session_media_get_track_info(void * hSession, int index) {
    if (hSession == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    return m3u_get_track_info(session->master_playlist, index);
}

int m3u_session_media_select_track(void * hSession, int index, int select, int64_t anchorTimeUs) {
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    if (m3u_select_track(session->master_playlist, index, select) < 0) {
        LOGE("[%s:%d] select track(%d) failed !", __FUNCTION__, __LINE__, index);
        return -1;
    }
    MediaType type = m3u_get_media_type_by_index(session->master_playlist, index);
    if (type == TYPE_NONE) {
        LOGE("[%s:%d] select track(%d) failed !", __FUNCTION__, __LINE__, index);
        return -1;
    }
    SessionMediaItem * item = NULL;
    int i = 0;
    for (; i < MEDIA_TYPE_NUM; i++) {
        if (session->media_item_array[i]->media_type == type) {
            item = session->media_item_array[i];
            break;
        }
    }
    if (!item) {
        LOGE("[%s:%d] select track(%d) failed !", __FUNCTION__, __LINE__, index);
        return -1;
    }
    item->media_handling_seek = 1;
    amthreadpool_pool_thread_cancel(item->media_tid);
    pthread_mutex_lock(&item->media_lock);
    pthread_cond_broadcast(&item->media_cond);
    item->media_seek_flag = 2; // select/unselect track.
    item->media_eof_flag = 0;
    item->media_switch_anchor_timeUs = anchorTimeUs;
    pthread_mutex_unlock(&item->media_lock);
    while (item->media_seek_flag > 0) {
        if (session->interrupt && (*session->interrupt)()) {
            break;
        }
        amthreadpool_thread_usleep(1000 * 10);
    }
    amthreadpool_thread_uncancel(item->media_tid);
#ifdef USE_SIMPLE_CACHE
    pthread_mutex_lock(&item->media_lock);
    hls_simple_cache_reset(item->media_cache);
    pthread_mutex_unlock(&item->media_lock);
#endif
    item->media_handling_seek = 0;
    return 0;
}

int m3u_session_media_get_selected_track(void * hSession, MediaTrackType type) {
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    return m3u_get_selected_track(session->master_playlist, type);
}

M3uSubtitleData * m3u_session_media_read_subtitle(void * hSession, int index) {
    if (hSession == NULL) {
        ERROR_MSG();
        return NULL;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    int selected_index = m3u_get_selected_track(session->master_playlist, M3U_MEDIA_TRACK_TYPE_SUBTITLE);
    if (selected_index < 0) {
        return NULL;
    }
    SessionMediaItem * mediaItem = session->media_item_array[index];
    if (!mediaItem->media_sub_ready) {
        return NULL;
    }
    int cached_sub_size = hls_simple_cache_get_data_size(mediaItem->media_cache);
    if (cached_sub_size <= 0) {
        return NULL;
    }
    int firstSeqNumberInPlaylist = m3u_get_node_by_index(mediaItem->media_playlist, 0)->media_sequence;
    M3uBaseNode * item = m3u_get_node_by_index(mediaItem->media_playlist, mediaItem->media_cur_seq_num - firstSeqNumberInPlaylist - 1);
    if (!item) {
        return NULL;
    }
    M3uSubtitleData * subData = (M3uSubtitleData *)malloc(sizeof(M3uSubtitleData));
    memset(subData, 0, sizeof(M3uSubtitleData));
    subData->sub_trackIndex = selected_index;
    subData->sub_timeUs = item->startUs;
    subData->sub_durationUs = item->durationUs;
    subData->sub_size = cached_sub_size;
    subData->sub_buffer = (uint8_t *)malloc(subData->sub_size);
    int read_size = hls_simple_cache_read(mediaItem->media_cache, subData->sub_buffer, subData->sub_size);
    if (read_size != subData->sub_size) {
        LOGE("[%s:%d] subtitle data not read completely, read size(%d), raw size(%d)", __FUNCTION__, __LINE__, read_size, subData->sub_size);
        free(subData->sub_buffer);
        free(subData);
        return NULL;
    }
    return subData;
}

MediaType m3u_session_media_get_type_by_index(void * hSession, int index) {
    if (hSession == NULL) {
        ERROR_MSG();
        return -1;
    }
    M3ULiveSession * session = (M3ULiveSession *)hSession;
    return m3u_get_media_type_by_index(session->master_playlist, index);
}
