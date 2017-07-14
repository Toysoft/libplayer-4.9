/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


/********************************************
 * name : player_control.c
 * function: thread manage
 * date     : 2010.2.22
 ********************************************/
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <player.h>

#include "player_priv.h"
#include "thread_mgt.h"
#include <amthreadpool.h>


int  player_thread_wait(play_para_t *player, int microseconds)
{
    return amthreadpool_thread_usleep(microseconds);
}


int wakeup_player_thread(play_para_t *player)
{
    return amthreadpool_thread_wake(player->thread_mgt.pthread_id);
}

player_status get_player_state(play_para_t *player)
{
    player_status status = PLAYER_INITING ;
    player_thread_mgt_t *mgt = &player->thread_mgt;
    if (mgt) {
        status = mgt->player_state;
    }
    return status;
}
void *player_thread_t(void *arg)
{
    void *ret;
    ret = player_thread((play_para_t*)arg);
    return ret;
}

int player_thread_create(play_para_t *player)
{
    int ret = 0;
    pthread_t       tid;
    pthread_attr_t pthread_attr;
    player_thread_mgt_t *mgt = &player->thread_mgt;

    pthread_attr_init(&pthread_attr);
    pthread_attr_setstacksize(&pthread_attr, 0);   //default stack size maybe better
    log_print("***player_para=%p,start_param=%p\n", player, player->start_param);
    ret = amthreadpool_pthread_create(&tid, &pthread_attr, (void*)&player_thread_t, (void*)player);
    if (ret != 0) {
        log_print("creat player thread failed !\n");
        return ret;
    }
    log_print("[player_thread_create:%d]creat thread success,tid=%lu\n", __LINE__, tid);
    pthread_setname_np(tid, "AmplayerMain");
    pthread_attr_destroy(&pthread_attr);
    mgt->pthread_id = tid;
    return PLAYER_SUCCESS;

}
int player_thread_wait_exit(play_para_t *player)
{
    int ret;
    player_thread_mgt_t *mgt = &player->thread_mgt;
    log_print("[player_thread_wait_exit:%d]pid=[%d] thead_id=%lu\n", __LINE__, player->player_id, mgt->pthread_id);
    if (mgt) {
        ret = amthreadpool_pthread_join(mgt->pthread_id, NULL);
    } else {
        ret = 0;
    }
    log_print("[player_thread_wait_exit:%d]thead_id=%lu returning\n", __LINE__, mgt->pthread_id);
    return ret;
}

#ifdef PLAYER_DEBUG
void debug_set_player_state(play_para_t *player, player_status status, const char *fn, int line)
{
    player_thread_mgt_t *mgt = &player->thread_mgt;
    log_print("\n************************Changed player state to %d,At %s:%d\n", status, fn, line);
    mgt->player_state = status;
}
#else
void set_player_state(play_para_t *player, player_status status)
{
    player_thread_mgt_t *mgt = &player->thread_mgt;
    mgt->player_state = status;
}
#endif

