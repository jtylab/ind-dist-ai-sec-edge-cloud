/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    CAN receiver module header
 */

#ifndef __CAN_RECEIVER_H__
#define __CAN_RECEIVER_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CAN接收数据结构体 */
typedef struct {
    rt_uint32_t can_id;              /* CAN ID */
    rt_uint8_t data[8];              /* 数据 */
    rt_uint8_t dlc;                  /* 数据长度 */
    rt_uint32_t timestamp;           /* 时间戳 */
} can_data_t;

/* CAN接收队列大小 */
#define CAN_RX_QUEUE_SIZE 32

/* 定义两个CAN ID（可根据实际修改） */
#define CAN_ID_1 0x001                 /* CAN ID 1 */
#define CAN_ID_2 0x002                 /* CAN ID 2 */

/**
 * 初始化CAN接收模块
 * @return 0 成功，其他值失败
 */
int can_receiver_init(void);

/**
 * 从接收缓冲中获取CAN数据
 * @param data 接收数据缓冲区指针
 * @param timeout 超时时间（毫秒）
 * @return 0 成功，其他值失败
 */
int can_receiver_get_data(can_data_t *data, rt_int32_t timeout);

/**
 * 启动CAN接收线程
 * @return 0 成功，其他值失败
 */
int can_receiver_start(void);

/**
 * 获取接收队列中的数据个数
 * @return 队列中的数据个数
 */
rt_uint32_t can_receiver_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_RECEIVER_H__ */
