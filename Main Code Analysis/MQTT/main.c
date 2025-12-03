/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    first version
 * 2025-12-02     RT-Thread    Add CAN, WiFi/MQTT, and SD storage modules
 */

#include <rtthread.h>
#include "can_receiver.h"
#include "mqtt_client.h"
#include "sd_storage.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 应用层主线程优先级 */
#define APP_MAIN_THREAD_PRIORITY 11

/* 数据转发线程堆栈大小 */
#define DATA_FORWARD_THREAD_STACK_SIZE 1024

/* MQTT配置 */
static mqtt_config_t mqtt_config = {
    .wifi_ssid = "Mate 60",                    /* WiFi SSID - 请根据实际修改 */
    .wifi_password = "12345678",               /* WiFi 密码 - 请根据实际修改 */
    .mqtt_broker = "test.mosquitto.org",      /* MQTT broker 地址 - 请根据实际修改 */
    .mqtt_port = 1883,                         /* MQTT 端口 */
    .mqtt_client_id = "stm32_client_001",      /* MQTT client ID */
    .mqtt_user = "",                           /* MQTT 用户名 */
    .mqtt_password = "",                       /* MQTT 密码 */
    .mqtt_publish_topic = "stm32/can/data"     /* MQTT 发布主题 */
};

/* 数据转发线程 - 负责从CAN接收数据并处理 */
static void data_forward_thread_entry(void *param)
{
    can_data_t can_data;
    sd_storage_record_t sd_record;
    rt_err_t err;

    LOG_I("Data forward thread started");

    while (1)
    {
        /* 从CAN接收缓冲中获取数据，超时时间100ms */
        err = can_receiver_get_data(&can_data, rt_tick_from_millisecond(100));
        if (err != 0)
        {
            /* 没有CAN数据，继续等待 */
            continue;
        }

        LOG_D("Received CAN data - ID: 0x%03X, DLC: %d", can_data.can_id, can_data.dlc);

        /* 检查WiFi连接状态 */
        if (mqtt_client_is_wifi_connected() && mqtt_client_get_status() == MQTT_STATUS_CONNECTED)
        {
            /* WiFi已连接，发送数据到MQTT */
            if (mqtt_client_publish_can_data(can_data.can_id, can_data.data, can_data.dlc) == 0)
            {
                LOG_D("CAN data published to MQTT successfully");
            }
            else
            {
                LOG_W("Failed to publish CAN data to MQTT");
                
                /* 发送失败，备份到SD卡 */
                sd_record.can_id = can_data.can_id;
                sd_record.dlc = can_data.dlc;
                sd_record.timestamp = can_data.timestamp;
                rt_memcpy(sd_record.data, can_data.data, can_data.dlc);
                
                sd_storage_save_record(&sd_record);
            }
        }
        else
        {
            /* WiFi未连接，保存数据到SD卡 */
            LOG_W("WiFi disconnected, saving CAN data to SD card");

            sd_record.can_id = can_data.can_id;
            sd_record.dlc = can_data.dlc;
            sd_record.timestamp = can_data.timestamp;
            rt_memcpy(sd_record.data, can_data.data, can_data.dlc);

            if (sd_storage_save_record(&sd_record) == 0)
            {
                LOG_D("CAN data saved to SD card buffer");
            }
            else
            {
                LOG_E("Failed to save CAN data to SD card");
            }
        }

        rt_thread_mdelay(10);
    }
}

int main(void)
{
    rt_thread_t data_forward_thread;
    rt_err_t err;

    LOG_I("==============================================");
    LOG_I("STM32F407ZGT6 Application Started");
    LOG_I("==============================================");
    LOG_I("Features:");
    LOG_I("  1. CAN Receiver with dual ID support");
    LOG_I("  2. WiFi/MQTT cloud connectivity");
    LOG_I("  3. SD card data backup on WiFi disconnect");
    LOG_I("==============================================");

    /* ==================== 初始化阶段 ==================== */
    
    /* 1. 初始化CAN接收模块 */
    LOG_I("Initializing CAN receiver module...");
    if (can_receiver_init() != 0)
    {
        LOG_E("Failed to initialize CAN receiver");
        return -1;
    }
    LOG_I("CAN receiver module initialized");

    /* 2. 初始化MQTT客户端模块 */
    LOG_I("Initializing MQTT client module...");
    if (mqtt_client_init(&mqtt_config) != 0)
    {
        LOG_E("Failed to initialize MQTT client");
        return -1;
    }
    LOG_I("MQTT client module initialized");

    /* 3. 初始化SD卡存储模块 */
    LOG_I("Initializing SD storage module...");
    if (sd_storage_init() != 0)
    {
        LOG_E("Failed to initialize SD storage");
        return -1;
    }
    LOG_I("SD storage module initialized");

    /* ==================== 启动阶段 ==================== */

    /* 1. 启动CAN接收线程 */
    LOG_I("Starting CAN receiver thread...");
    if (can_receiver_start() != 0)
    {
        LOG_E("Failed to start CAN receiver");
        return -1;
    }
    LOG_I("CAN receiver thread started");

    /* 2. 启动MQTT客户端线程 */
    LOG_I("Starting MQTT client thread...");
    if (mqtt_client_start() != 0)
    {
        LOG_E("Failed to start MQTT client");
        return -1;
    }
    LOG_I("MQTT client thread started");

    /* 3. 启动SD卡存储线程 */
    LOG_I("Starting SD storage thread...");
    if (sd_storage_start() != 0)
    {
        LOG_E("Failed to start SD storage");
        return -1;
    }
    LOG_I("SD storage thread started");

    /* 4. 创建数据转发线程 */
    LOG_I("Starting data forward thread...");
    data_forward_thread = rt_thread_create(
        "data_forward",
        data_forward_thread_entry,
        RT_NULL,
        DATA_FORWARD_THREAD_STACK_SIZE,
        APP_MAIN_THREAD_PRIORITY,
        10
    );

    if (data_forward_thread == RT_NULL)
    {
        LOG_E("Failed to create data forward thread");
        return -1;
    }

    err = rt_thread_startup(data_forward_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start data forward thread");
        return -1;
    }
    LOG_I("Data forward thread started");

    LOG_I("==============================================");
    LOG_I("All modules started successfully!");
    LOG_I("Application running, waiting for CAN data...");
    LOG_I("==============================================");

    /* ==================== 运行阶段 ==================== */

    /* 主线程监控 */
    while (1)
    {
        rt_thread_mdelay(5000);

        /* 定期输出状态信息 */
        LOG_D("System Status:");
        LOG_D("  - CAN RX Queue: %d", can_receiver_get_count());
        LOG_D("  - SD Buffer: %d records", sd_storage_get_buffer_count());
        LOG_D("  - MQTT Status: %d", mqtt_client_get_status());
        LOG_D("  - WiFi Connected: %s", mqtt_client_is_wifi_connected() ? "Yes" : "No");
    }

    return RT_EOK;
}
