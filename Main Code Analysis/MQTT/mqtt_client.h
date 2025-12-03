/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    MQTT client module header
 */

#ifndef __MQTT_CLIENT_H__
#define __MQTT_CLIENT_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT连接状态枚举 */
typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,    /* 未连接 */
    MQTT_STATUS_CONNECTING,          /* 连接中 */
    MQTT_STATUS_CONNECTED,           /* 已连接 */
    MQTT_STATUS_RECONNECTING         /* 重新连接中 */
} mqtt_status_t;

/* MQTT配置结构体 */
typedef struct {
    const char *wifi_ssid;           /* WiFi SSID */
    const char *wifi_password;       /* WiFi 密码 */
    const char *mqtt_broker;         /* MQTT broker地址 */
    rt_uint16_t mqtt_port;           /* MQTT port */
    const char *mqtt_client_id;      /* MQTT client ID */
    const char *mqtt_user;           /* MQTT 用户名 */
    const char *mqtt_password;       /* MQTT 密码 */
    const char *mqtt_publish_topic;  /* MQTT 发布主题 */
} mqtt_config_t;

/* MQTT客户端结构体 */
typedef struct {
    mqtt_config_t config;            /* 配置 */
    mqtt_status_t status;            /* 连接状态 */
    rt_device_t uart_dev;            /* UART设备指针 */
    rt_thread_t mqtt_thread;         /* MQTT线程 */
    rt_thread_t wifi_monitor_thread; /* WiFi监控线程 */
    rt_uint32_t reconnect_count;     /* 重连次数 */
} mqtt_client_t;

/**
 * 初始化MQTT客户端模块
 * @param config MQTT配置
 * @return 0 成功，其他值失败
 */
int mqtt_client_init(mqtt_config_t *config);

/**
 * 启动MQTT服务
 * @return 0 成功，其他值失败
 */
int mqtt_client_start(void);

/**
 * 发布MQTT消息
 * @param topic 主题
 * @param payload 消息内容
 * @param payload_len 消息长度
 * @return 0 成功，其他值失败
 */
int mqtt_client_publish(const char *topic, const void *payload, rt_size_t payload_len);

/**
 * 发送CAN数据到MQTT
 * @param can_id CAN ID
 * @param data 数据指针
 * @param len 数据长度
 * @return 0 成功，其他值失败
 */
int mqtt_client_publish_can_data(rt_uint32_t can_id, const rt_uint8_t *data, rt_uint8_t len);

/**
 * 获取MQTT连接状态
 * @return 当前连接状态
 */
mqtt_status_t mqtt_client_get_status(void);

/**
 * 检查是否连接到WiFi
 * @return RT_TRUE 已连接，RT_FALSE 未连接
 */
rt_bool_t mqtt_client_is_wifi_connected(void);

/**
 * 停止MQTT服务
 * @return 0 成功，其他值失败
 */
int mqtt_client_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_CLIENT_H__ */
