/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    SD storage module implementation
 */

#include "sd_storage.h"
#include <rtdevice.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define DBG_TAG "sd_storage"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <sys/statfs.h>

/* SD卡存储状态 */
static sd_status_t sd_status = SD_STATUS_UNMOUNTED;

/* SD卡存储线程 */
static rt_thread_t sd_storage_thread = RT_NULL;

/* 存储缓冲区 */
static sd_storage_record_t sd_buffer[SD_BUFFER_SIZE];
static rt_uint32_t sd_buffer_count = 0;

/* 缓冲区访问互斥锁 */
static rt_mutex_t sd_buffer_lock = RT_NULL;

/* 数据可用信号量 */
static rt_sem_t sd_data_sem = RT_NULL;

/* SD卡存储线程优先级 */
#define SD_STORAGE_THREAD_PRIORITY 14

/* SD卡存储线程堆栈大小 */
#define SD_STORAGE_THREAD_STACK_SIZE 1024

/* 文件操作超时 */
#define FILE_OP_TIMEOUT 5000

/**
 * 挂载SD卡
 */
static int mount_sd_card(void)
{
    struct statfs stat;
    int ret;

    LOG_I("Mounting SD card");

    /* 检查SD卡是否已经挂载 */
    if (statfs(SD_MOUNT_POINT, &stat) == 0)
    {
        LOG_I("SD card already mounted at %s", SD_MOUNT_POINT);
        sd_status = SD_STATUS_MOUNTED;
        return 0;
    }

    /* 尝试挂载SD卡 */
    ret = dfs_mount("sd0", SD_MOUNT_POINT, "elm", 0, 0);
    if (ret == 0)
    {
        LOG_I("SD card mounted successfully at %s", SD_MOUNT_POINT);
        sd_status = SD_STATUS_MOUNTED;
        return 0;
    }
    else
    {
        LOG_E("Failed to mount SD card: %d", ret);
        sd_status = SD_STATUS_ERROR;
        return -1;
    }
}

/**
 * 卸载SD卡
 */
static int unmount_sd_card(void)
{
    int ret;

    LOG_I("Unmounting SD card");

    ret = dfs_unmount(SD_MOUNT_POINT);
    if (ret == 0)
    {
        LOG_I("SD card unmounted successfully");
        sd_status = SD_STATUS_UNMOUNTED;
        return 0;
    }
    else
    {
        LOG_W("Failed to unmount SD card: %d", ret);
        return -1;
    }
}

/**
 * 初始化SD卡文件
 */
static int init_sd_files(void)
{
    int fd;

    /* 创建数据文件 */
    fd = open(SD_DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        LOG_E("Failed to create/open data file: %s", SD_DATA_FILE);
        return -1;
    }

    close(fd);
    LOG_D("Data file initialized: %s", SD_DATA_FILE);

    return 0;
}

/**
 * 将缓冲区数据写入SD卡
 */
static int flush_buffer_to_sd(void)
{
    int fd;
    rt_size_t written;
    rt_uint32_t i;

    if (sd_buffer_count == 0)
    {
        return 0;
    }

    if (sd_status != SD_STATUS_MOUNTED)
    {
        LOG_W("SD card not mounted, cannot flush buffer");
        return -1;
    }

    /* 获取互斥锁 */
    rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);

    /* 打开文件进行写操作 */
    fd = open(SD_DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        LOG_E("Failed to open data file for writing");
        rt_mutex_release(sd_buffer_lock);
        return -1;
    }

    /* 写入所有缓冲区数据 */
    for (i = 0; i < sd_buffer_count; i++)
    {
        written = write(fd, &sd_buffer[i], sizeof(sd_storage_record_t));
        if (written != sizeof(sd_storage_record_t))
        {
            LOG_W("Failed to write record %d to SD card", i);
            break;
        }
    }

    close(fd);

    LOG_I("Flushed %d records to SD card", i);

    /* 清空缓冲区 */
    sd_buffer_count = 0;

    rt_mutex_release(sd_buffer_lock);

    return i;
}

/**
 * SD卡存储线程
 */
static void sd_storage_thread_entry(void *param)
{
    rt_err_t err;
    rt_uint32_t flush_interval = 10000;  /* 10秒 */

    LOG_I("SD storage thread started");

    /* 挂载SD卡 */
    if (mount_sd_card() != 0)
    {
        LOG_E("Failed to mount SD card at startup");
        /* 可选：继续运行，等待SD卡插入 */
    }

    /* 初始化SD文件 */
    if (sd_status == SD_STATUS_MOUNTED)
    {
        init_sd_files();
    }

    /* 主循环 */
    while (1)
    {
        /* 等待数据可用或超时 */
        err = rt_sem_take(sd_data_sem, rt_tick_from_millisecond(flush_interval));

        /* 周期性或有数据时刷新缓冲区 */
        if (sd_status == SD_STATUS_MOUNTED && sd_buffer_count > 0)
        {
            flush_buffer_to_sd();
        }
        else if (sd_status == SD_STATUS_ERROR || sd_status == SD_STATUS_UNMOUNTED)
        {
            /* 尝试重新挂载SD卡 */
            if (mount_sd_card() == 0)
            {
                init_sd_files();
                flush_buffer_to_sd();
            }
        }
    }
}

/**
 * 初始化SD卡存储模块
 */
int sd_storage_init(void)
{
    rt_err_t err;

    LOG_I("Initializing SD storage module");

    /* 创建互斥锁 */
    if (sd_buffer_lock == RT_NULL)
    {
        sd_buffer_lock = rt_mutex_create("sd_buffer_lock", RT_IPC_FLAG_FIFO);
        if (sd_buffer_lock == RT_NULL)
        {
            LOG_E("Failed to create buffer lock");
            return -1;
        }
    }

    /* 创建信号量 */
    if (sd_data_sem == RT_NULL)
    {
        sd_data_sem = rt_sem_create("sd_data_sem", 0, RT_IPC_FLAG_FIFO);
        if (sd_data_sem == RT_NULL)
        {
            LOG_E("Failed to create data semaphore");
            return -1;
        }
    }

    /* 初始化缓冲区 */
    sd_buffer_count = 0;
    rt_memset(sd_buffer, 0, sizeof(sd_buffer));

    LOG_I("SD storage module initialized successfully");
    return 0;
}

/**
 * 启动SD卡存储线程
 */
int sd_storage_start(void)
{
    rt_err_t err;

    if (sd_storage_thread != RT_NULL)
    {
        LOG_W("SD storage thread already started");
        return 0;
    }

    /* 创建存储线程 */
    sd_storage_thread = rt_thread_create(
        "sd_storage",
        sd_storage_thread_entry,
        RT_NULL,
        SD_STORAGE_THREAD_STACK_SIZE,
        SD_STORAGE_THREAD_PRIORITY,
        10
    );

    if (sd_storage_thread == RT_NULL)
    {
        LOG_E("Failed to create SD storage thread");
        return -1;
    }

    err = rt_thread_startup(sd_storage_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start SD storage thread: %d", err);
        return -1;
    }

    LOG_I("SD storage thread started successfully");
    return 0;
}

/**
 * 保存CAN数据到SD卡
 */
int sd_storage_save_record(sd_storage_record_t *record)
{
    rt_err_t err;

    if (record == RT_NULL)
    {
        return -1;
    }

    /* 获取互斥锁 */
    err = rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);
    if (err != RT_EOK)
    {
        return -1;
    }

    /* 检查缓冲区是否满 */
    if (sd_buffer_count >= SD_BUFFER_SIZE)
    {
        LOG_W("SD buffer full, data will be lost");
        rt_mutex_release(sd_buffer_lock);
        return -1;
    }

    /* 添加到缓冲区 */
    rt_memcpy(&sd_buffer[sd_buffer_count], record, sizeof(sd_storage_record_t));
    sd_buffer_count++;

    rt_mutex_release(sd_buffer_lock);

    /* 释放信号量，通知存储线程 */
    if (sd_buffer_count >= SD_BUFFER_SIZE / 2)  /* 缓冲区半满时 */
    {
        rt_sem_release(sd_data_sem);
    }

    return 0;
}

/**
 * 保存多条CAN数据到SD卡
 */
int sd_storage_save_batch(sd_storage_record_t *records, rt_uint32_t count)
{
    rt_uint32_t i;
    int ret;

    if (records == RT_NULL || count == 0)
    {
        return 0;
    }

    /* 逐条保存记录 */
    for (i = 0; i < count; i++)
    {
        ret = sd_storage_save_record(&records[i]);
        if (ret != 0)
        {
            LOG_W("Failed to save record %d", i);
            break;
        }
    }

    return i;
}

/**
 * 读取SD卡中的数据
 */
int sd_storage_read_records(sd_storage_record_t *records, rt_uint32_t max_count)
{
    int fd;
    rt_size_t read_size;
    rt_uint32_t count = 0;

    if (records == RT_NULL || max_count == 0)
    {
        return 0;
    }

    if (sd_status != SD_STATUS_MOUNTED)
    {
        LOG_W("SD card not mounted");
        return 0;
    }

    /* 打开数据文件 */
    fd = open(SD_DATA_FILE, O_RDONLY, 0644);
    if (fd < 0)
    {
        LOG_W("Failed to open data file for reading");
        return 0;
    }

    /* 读取记录 */
    while (count < max_count)
    {
        read_size = read(fd, &records[count], sizeof(sd_storage_record_t));
        if (read_size != sizeof(sd_storage_record_t))
        {
            break;
        }
        count++;
    }

    close(fd);

    LOG_I("Read %d records from SD card", count);

    return count;
}

/**
 * 获取SD卡状态
 */
sd_status_t sd_storage_get_status(void)
{
    return sd_status;
}

/**
 * 检查SD卡是否已挂载
 */
rt_bool_t sd_storage_is_mounted(void)
{
    return (sd_status == SD_STATUS_MOUNTED) ? RT_TRUE : RT_FALSE;
}

/**
 * 获取缓冲区中待保存的记录数
 */
rt_uint32_t sd_storage_get_buffer_count(void)
{
    rt_uint32_t count;

    rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);
    count = sd_buffer_count;
    rt_mutex_release(sd_buffer_lock);

    return count;
}

/**
 * 停止SD卡存储模块
 */
int sd_storage_stop(void)
{
    /* 刷新所有缓冲区数据到SD卡 */
    if (sd_status == SD_STATUS_MOUNTED)
    {
        flush_buffer_to_sd();
    }

    /* 卸载SD卡 */
    unmount_sd_card();

    LOG_I("SD storage module stopped");
    return 0;
}
