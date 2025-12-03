/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    SD storage module header
 */

#ifndef __SD_STORAGE_H__
#define __SD_STORAGE_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SD卡存储数据结构体 */
typedef struct {
    rt_uint32_t can_id;              /* CAN ID */
    rt_uint8_t data[8];              /* 数据 */
    rt_uint8_t dlc;                  /* 数据长度 */
    rt_uint32_t timestamp;           /* 时间戳 */
} sd_storage_record_t;

/* SD卡挂载点 */
#define SD_MOUNT_POINT "/sd"

/* SD卡数据文件路径 */
#define SD_DATA_FILE "/sd/can_data.bin"

/* SD卡日志文件路径 */
#define SD_LOG_FILE "/sd/system.log"

/* SD卡缓冲区大小 */
#define SD_BUFFER_SIZE 256

/* SD存储状态枚举 */
typedef enum {
    SD_STATUS_UNMOUNTED = 0,         /* 未挂载 */
    SD_STATUS_MOUNTED,               /* 已挂载 */
    SD_STATUS_ERROR                  /* 错误 */
} sd_status_t;

/**
 * 初始化SD卡存储模块
 * @return 0 成功，其他值失败
 */
int sd_storage_init(void);

/**
 * 启动SD卡存储线程
 * @return 0 成功，其他值失败
 */
int sd_storage_start(void);

/**
 * 保存CAN数据到SD卡
 * @param record 要保存的数据记录
 * @return 0 成功，其他值失败
 */
int sd_storage_save_record(sd_storage_record_t *record);

/**
 * 保存多条CAN数据到SD卡
 * @param records 数据记录数组
 * @param count 记录数量
 * @return 实际保存的记录数
 */
int sd_storage_save_batch(sd_storage_record_t *records, rt_uint32_t count);

/**
 * 读取SD卡中的数据
 * @param records 接收数据的缓冲区
 * @param max_count 最多读取的记录数
 * @return 实际读取的记录数
 */
int sd_storage_read_records(sd_storage_record_t *records, rt_uint32_t max_count);

/**
 * 获取SD卡状态
 * @return 当前状态
 */
sd_status_t sd_storage_get_status(void);

/**
 * 检查SD卡是否已挂载
 * @return RT_TRUE 已挂载，RT_FALSE 未挂载
 */
rt_bool_t sd_storage_is_mounted(void);

/**
 * 获取缓冲区中待保存的记录数
 * @return 记录数
 */
rt_uint32_t sd_storage_get_buffer_count(void);

/**
 * 停止SD卡存储模块
 * @return 0 成功，其他值失败
 */
int sd_storage_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_STORAGE_H__ */
