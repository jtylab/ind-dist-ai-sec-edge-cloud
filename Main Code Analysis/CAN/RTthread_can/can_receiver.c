/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    CAN receiver module implementation
 */

#include "can_receiver.h"
#include <rtdevice.h>
#include <string.h>

#define DBG_TAG "can_receiver"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* CAN设备指针 */
static rt_device_t can_dev = RT_NULL;

/* CAN接收消息队列 */
static struct rt_messagequeue *can_rx_queue = RT_NULL;

/* CAN接收线程 */
static rt_thread_t can_rx_thread = RT_NULL;

/* CAN接收线程优先级 */
#define CAN_RX_THREAD_PRIORITY 15

/* CAN接收线程堆栈大小 */
#define CAN_RX_THREAD_STACK_SIZE 1024

/**
 * CAN接收回调函数
 */
static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size)
{
    LOG_D("CAN receive callback, size: %d", size);
    return RT_EOK;
}

/**
 * CAN接收线程函数
 */
static void can_receiver_thread_entry(void *param)
{
    struct rt_can_msg rx_msg;
    can_data_t can_data;
    rt_size_t size;
    rt_err_t err;

    LOG_I("CAN receiver thread started");

    /* 打开CAN设备 */
    if (can_dev == RT_NULL)
    {
        can_dev = rt_device_find("can0");
        if (can_dev == RT_NULL)
        {
            LOG_E("CAN device not found");
            return;
        }

        err = rt_device_open(can_dev, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
        if (err != RT_EOK)
        {
            LOG_E("Failed to open CAN device: %d", err);
            return;
        }

        /* 设置接收回调函数 */
        rt_device_set_rx_indicate(can_dev, can_rx_callback);
    }

    /* 接收循环 */
    while (1)
    {
        /* 读取CAN数据 */
        size = rt_device_read(can_dev, 0, &rx_msg, sizeof(struct rt_can_msg));

        if (size == sizeof(struct rt_can_msg))
        {
            /* 检查是否是目标CAN ID */
            if ((rx_msg.id == CAN_ID_1) || (rx_msg.id == CAN_ID_2))
            {
                /* 构造CAN数据 */
                can_data.can_id = rx_msg.id;
                can_data.dlc = rx_msg.len;
                can_data.timestamp = rt_tick_get();

                /* 复制数据 */
                rt_memcpy(can_data.data, rx_msg.data, rx_msg.len);

                /* 发送到消息队列 */
                err = rt_mq_send(can_rx_queue, &can_data, sizeof(can_data_t));
                if (err != RT_EOK)
                {
                    LOG_W("CAN message queue full, data lost");
                }

                LOG_D("CAN ID: 0x%03X, DLC: %d", rx_msg.id, rx_msg.len);
            }
        }
        else
        {
            /* 延迟等待 */
            rt_thread_mdelay(10);
        }
    }
}

/**
 * 初始化CAN接收模块
 */
int can_receiver_init(void)
{
    rt_err_t err;

    LOG_I("Initializing CAN receiver module");

    /* 创建消息队列 */
    can_rx_queue = rt_mq_create("can_queue", sizeof(can_data_t), CAN_RX_QUEUE_SIZE, RT_IPC_FLAG_FIFO);
    if (can_rx_queue == RT_NULL)
    {
        LOG_E("Failed to create CAN message queue");
        return -1;
    }

    LOG_I("CAN receiver module initialized successfully");
    return 0;
}

/**
 * 启动CAN接收线程
 */
int can_receiver_start(void)
{
    rt_err_t err;

    if (can_rx_thread != RT_NULL)
    {
        LOG_W("CAN receiver thread already started");
        return 0;
    }

    /* 创建接收线程 */
    can_rx_thread = rt_thread_create(
        "can_rx",
        can_receiver_thread_entry,
        RT_NULL,
        CAN_RX_THREAD_STACK_SIZE,
        CAN_RX_THREAD_PRIORITY,
        10
    );

    if (can_rx_thread == RT_NULL)
    {
        LOG_E("Failed to create CAN receiver thread");
        return -1;
    }

    /* 启动线程 */
    err = rt_thread_startup(can_rx_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start CAN receiver thread: %d", err);
        return -1;
    }

    LOG_I("CAN receiver thread started successfully");
    return 0;
}

/**
 * 从接收缓冲中获取CAN数据
 */
int can_receiver_get_data(can_data_t *data, rt_int32_t timeout)
{
    rt_err_t err;

    if (data == RT_NULL)
    {
        return -1;
    }

    if (can_rx_queue == RT_NULL)
    {
        return -1;
    }

    /* 从消息队列接收数据 */
    err = rt_mq_recv(can_rx_queue, data, sizeof(can_data_t), timeout);
    if (err != RT_EOK)
    {
        return -1;
    }

    return 0;
}

/**
 * 获取接收队列中的数据个数
 */
rt_uint32_t can_receiver_get_count(void)
{
    if (can_rx_queue == RT_NULL)
    {
        return 0;
    }

    return can_rx_queue->entry;
}
