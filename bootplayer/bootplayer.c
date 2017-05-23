
#define LOG_TAG "bootplayer"

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#ifdef ANDROID
#include <cutils/log.h>
#include <cutils/properties.h>
#endif
#include "player.h"
#include "player_type.h"
#include "streamsource.h"
#include "Amsysfsutils.h"

#ifndef ANDROID
#define ALOGD printf
#define ALOGE printf
#define ALOGI printf
#endif

typedef enum {
    //EMU_STEP_NONE = 0,
    EMU_STEP_PAUSE = 2,
    EMU_STEP_RESUME = 3,
    EMU_STEP_START = 4,
    EMU_STEP_FF = 5,
    EMU_STEP_RR = 6,
    EMU_STEP_SEEK = 7,
    //EMU_STEP_MUTE= 8,
    //EMU_STEP_SETVOL=9,
    //EMU_STEP_GETVOL = 10,
    //EMU_STEP_SETTONE= 11,
    EMU_STEP_SETLOOP = 14,
    EMU_STEP_STOP = 16,
    //EMU_STEP_SPECTRUM = 17,
    //EMU_STEP_SETSUBT = 19,
    EMU_STEP_MENU = 20,
    EMU_STEP_EXIT = 21,
    //EMU_STEP_ATRACK = 22,
    EMU_STEP_GETAVMEDIAINFO = 25,
    //EMU_STEP_LISTALLMEDIAID = 27,
} EMU_STEP;

int update_player_info(int pid, player_info_t * info)
{
    ALOGD("pid:%d, status:%x,current pos:%d, total:%d, errcode:%x\n",
        pid, info->status, info->current_time, info->full_time, ~(info->error_no));
    return 0;
}

int _media_info_dump(media_info_t* minfo)
{
    int i = 0;

    ALOGD("file size:%lld, type:%d\n", minfo->stream_info.file_size, minfo->stream_info.type);
    ALOGD("has internal subtitle?:%s\n", minfo->stream_info.has_sub > 0 ? "YES!" : "NO!");
    ALOGD("internal subtile counts:%d\n", minfo->stream_info.total_sub_num);
    ALOGD("has video track?:%s\n", minfo->stream_info.has_video > 0 ? "YES!" : "NO!");
    ALOGD("has audio track?:%s\n", minfo->stream_info.has_audio > 0 ? "YES!" : "NO!");
    ALOGD("duration:%d\n", minfo->stream_info.duration);
    if (minfo->stream_info.has_video && minfo->stream_info.total_video_num > 0) {
        ALOGD("video counts:%d, width:%d, height:%d, bitrate:%d, format:%d\n",
            minfo->stream_info.total_video_num,
            minfo->video_info[0]->width,
            minfo->video_info[0]->height,
            minfo->video_info[0]->bit_rate,
            minfo->video_info[0]->format);
    }

    if (minfo->stream_info.has_audio && minfo->stream_info.total_audio_num > 0) {
        ALOGD("audio counts:%d\n", minfo->stream_info.total_audio_num);

        if (NULL != minfo->audio_info[0]->audio_tag) {
            ALOGD("track title:%s", minfo->audio_info[0]->audio_tag->title != NULL ? minfo->audio_info[0]->audio_tag->title : "unknow");
            ALOGD("track album:%s", minfo->audio_info[0]->audio_tag->album != NULL ? minfo->audio_info[0]->audio_tag->album : "unknow");
            ALOGD("track author:%s\n", minfo->audio_info[0]->audio_tag->author != NULL ? minfo->audio_info[0]->audio_tag->author : "unknow");
            ALOGD("track year:%s\n", minfo->audio_info[0]->audio_tag->year != NULL ? minfo->audio_info[0]->audio_tag->year : "unknow");
            ALOGD("track comment:%s\n", minfo->audio_info[0]->audio_tag->comment != NULL ? minfo->audio_info[0]->audio_tag->comment : "unknow");
            ALOGD("track genre:%s\n", minfo->audio_info[0]->audio_tag->genre != NULL ? minfo->audio_info[0]->audio_tag->genre : "unknow");
            ALOGD("track copyright:%s\n", minfo->audio_info[0]->audio_tag->copyright != NULL ? minfo->audio_info[0]->audio_tag->copyright : "unknow");
            ALOGD("track track:%d\n", minfo->audio_info[0]->audio_tag->track);
        }

        for (i = 0; i < minfo->stream_info.total_audio_num; i++) {
            ALOGD("%d 'st audio track codec type:%d\n", i, minfo->audio_info[i]->aformat);
            ALOGD("%d 'st audio track audio_channel:%d\n", i, minfo->audio_info[i]->channel);
            ALOGD("%d 'st audio track bit_rate:%d\n", i, minfo->audio_info[i]->bit_rate);
            ALOGD("%d 'st audio track audio_samplerate:%d\n", i, minfo->audio_info[i]->sample_rate);
        }
    }

    if (minfo->stream_info.has_sub && minfo->stream_info.total_sub_num > 0) {
        for (i = 0; i < minfo->stream_info.total_sub_num; i++) {
            if (0 == minfo->sub_info[i]->internal_external) {
                ALOGD("%d 'st internal subtitle pid:%d\n", i, minfo->sub_info[i]->id);
                ALOGD("%d 'st internal subtitle language:%s\n", i, minfo->sub_info[i]->sub_language ? minfo->sub_info[i]->sub_language : "unknow");
                ALOGD("%d 'st internal subtitle width:%d\n", i, minfo->sub_info[i]->width);
                ALOGD("%d 'st internal subtitle height:%d\n", i, minfo->sub_info[i]->height);
                ALOGD("%d 'st internal subtitle resolution:%d\n", i, minfo->sub_info[i]->resolution);
                ALOGD("%d 'st internal subtitle subtitle size:%lld\n", i, minfo->sub_info[i]->subtitle_size);
            }
        }
    }
    return 0;
}

static int get_axis(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;

    if (!startp) {
        return 0;
    }

    len = strlen(startp);

    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }

        if (len == 0) {
            break;
        }

        *out++ = strtol(startp, &endp, 0);

        len -= endp - startp;
        startp = endp;
        count++;

    } while ((endp) && (count < para_num) && (len > 0));

    return count;
}

static int set_display_axis()
{
    char *videoaxis_patch = "/sys/class/video/axis";
    char *videodisable_patch = "/sys/class/video/disable_video";
    char *videoscreenmode_patch = "/sys/class/video/screen_mode";

    amsysfs_set_sysfs_str(videoaxis_patch, "0 0 -1 -1");
    amsysfs_set_sysfs_str(videoscreenmode_patch, "1");
    amsysfs_set_sysfs_str(videodisable_patch, "2");
    return 0;
}

static int set_osd_blank()
{
    char *path1 = "/sys/class/graphics/fb0/blank";
    char *path2 = "/sys/class/graphics/fb1/blank";

    amsysfs_set_sysfs_str(path1, "1");
    amsysfs_set_sysfs_str(path2, "1");
    return 0;
}

static void signal_handler(int signum)
{
    ALOGD("Get signum=%x\n", signum);
    player_progress_exit();
    signal(signum, SIG_DFL);
    raise(signum);
    property_set("media.bootplayer.running", "0");
}

int main(int argc, char *argv[])
{
    int pid;
    int tmpneedexit = 0;
    media_info_t minfo;
    int osd_is_blank = 0;
    EMU_STEP tmpstep = EMU_STEP_MENU;
    char *di_bypass_all_path = "/sys/module/di/parameters/bypass_all";

    play_control_t *pCtrl = (play_control_t*)malloc(sizeof(play_control_t));
    memset(pCtrl, 0, sizeof(play_control_t));
    memset(&minfo, 0, sizeof(media_info_t));
    if (argc < 2) {
        ALOGE("can not get video file name. usage:bootplayer file\n");
        return -1;
    }

    ALOGD("file name:%s\n", argv[1]);

    amsysfs_set_sysfs_str(di_bypass_all_path, "1");

    player_init();
    //set_display_axis();      //move osd out of screen to set video layer out

    player_register_update_callback(&pCtrl->callback_fn, &update_player_info, 1000);

    pCtrl->file_name = strdup(argv[1]);

    //pCtrl->nosound = 1;   // if disable audio...,must call this api
    pCtrl->video_index = -1;// MUST
    pCtrl->audio_index = -1;// MUST
    pCtrl->sub_index = -1;/// MUST
    // pCtrl->hassub = 1;  // enable subtitle
    pCtrl->displast_frame = 1;/// MUST
    //just open a buffer,just for p2p,http,etc...
    //pCtrl->auto_buffing_enable = 1;
    //pCtrl->buffing_min = 0.001;
    //pCtrl->buffing_middle = 0.02;
    //pCtrl->buffing_max = 0.9;

    pCtrl->t_pos = -1;  // start position, if live streaming, need set to -1
    pCtrl->need_start = 0; // if 0,you can omit player_start_play API.just play video/audio immediately. if 1,need call "player_start_play" API;
    property_set("media.bootplayer.running", "1");

    pid = player_start(pCtrl, 0);
    if (pid < 0) {
        ALOGE("player start failed!error=%d\n", pid);
        return -1;
    }

    signal(SIGSEGV, signal_handler);
    //SYS_disable_osd0();
    set_display_axis();
    while ((!tmpneedexit) && (!PLAYER_THREAD_IS_STOPPED(player_get_state(pid)))) {
        if (!osd_is_blank) {
            int new_frame_count = amsysfs_get_sysfs_int16("/sys/module/amvideo/parameters/new_frame_count");
            int disable_video = amsysfs_get_sysfs_int16("/sys/class/video/disable_video");
            if (new_frame_count > 0 && (disable_video == 0)) {
                usleep(30000);
                set_osd_blank();
                osd_is_blank = 1;
                property_set("service.bootanim.shown", "1");
            }
        }
        switch (tmpstep) {
        case EMU_STEP_PAUSE:
            ALOGD("step pause\n");
            player_pause(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_RESUME:
            ALOGD("step resume\n");
            player_resume(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_SEEK:
            ALOGD("will seek position:100\n");
            player_timesearch(pid, 100);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_STOP:
            ALOGD("step stop\n");
            player_stop(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_FF:
            ALOGD("please input fastforward speed\n");
            player_forward(pid, 1);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_RR:
            ALOGD("please input fastrewind speed");
            player_backward(pid, 1);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_SETLOOP:
            ALOGD("step setloop\n");
            player_loop(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_EXIT:
            ALOGD("step exit\n");
            player_exit(pid);
            tmpneedexit = 1;
            break;
        case EMU_STEP_START:
            ALOGD("step start\n");
            player_start_play(pid);
            //SYS_set_tsync_enable(0);///< f no sound,can set to be 0
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_GETAVMEDIAINFO:
            if (pid >= 0) {
                ALOGD("step get av media info\n");
                if (player_get_state(pid) > PLAYER_INITOK) {
                    int ret = player_get_media_info(pid, &minfo);
                    if (ret == 0) {
                        _media_info_dump(&minfo);
                    }
                }
            }
            tmpstep = EMU_STEP_MENU;
            break;
        default:
            break;
        }

        usleep(100 * 1000);
        signal(SIGCHLD, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGHUP, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGSEGV, signal_handler);
        signal(SIGINT, signal_handler);
        signal(SIGQUIT, signal_handler);
    }
    free(pCtrl->file_name);
    free(pCtrl);
    ALOGD("player finish\n");
    amsysfs_set_sysfs_str(di_bypass_all_path, "0");
    amsysfs_set_sysfs_str("/sys/class/video/screen_mode", "0");
    property_set("media.bootplayer.running", "0");
    return 0;
}

