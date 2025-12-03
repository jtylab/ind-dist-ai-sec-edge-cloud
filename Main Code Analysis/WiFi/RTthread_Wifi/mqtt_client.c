/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    MQTT client module implementation
 */

#include "mqtt_client.h"
#include <rtdevice.h>
#include <string.h>
#include <stdio.h>


#define DBG_TAG "mqtt_client"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* MQTT客户端全局结构体 */
static mqtt_client_t mqtt_client = {0};

/* MQTT线程优先级 */
#define MQTT_THREAD_PRIORITY 12

/* MQTT线程堆栈大小 */
#define MQTT_THREAD_STACK_SIZE 2048

/* WiFi监控线程堆栈大小 */
#define WIFI_MONITOR_STACK_SIZE 1024

/* MQTT重连超时时间（秒） */
#define MQTT_RECONNECT_TIMEOUT 30

/* MQTT连接超时时间（秒） */
#define MQTT_CONNECT_TIMEOUT 10

/* AT指令缓冲区大小 */
#define AT_CMD_BUF_SIZE 256

/* UART接收缓冲区 */
static rt_uint8_t uart_rx_buffer[512] = {0};

/* UART接收数据标志 */
static rt_sem_t uart_rx_sem = RT_NULL;

/* MQTT连接标志 */
static rt_event_t mqtt_event = RT_NULL;

#define MQTT_EVENT_CONNECTED (1 << 0)
#define MQTT_EVENT_DISCONNECTED (1 << 1)

/**
 * UART接收回调函数
 */
static rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size)
{
    if (uart_rx_sem != RT_NULL)
    {
        rt_sem_release(uart_rx_sem);
    }
    return RT_EOK;
}

/**
 * 发送AT指令到ESP8266
 */
static int at_send_command(const char *cmd, char *resp, int resp_len, int timeout_ms)
{
    rt_size_t write_len;
    rt_size_t read_len;
    rt_err_t err;

    if (mqtt_client.uart_dev == RT_NULL)
    {
        LOG_E("UART device not initialized");
        return -1;
    }

    LOG_D("AT CMD: %s", cmd);

    /* 清空接收缓冲 */
    rt_memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));

    /* 发送AT指令 */
    write_len = rt_device_write(mqtt_client.uart_dev, 0, (const void *)cmd, rt_strlen(cmd));
    if (write_len != rt_strlen(cmd))
    {
        LOG_E("Failed to send AT command");
        return -1;
    }

    /* 等待接收响应 */
    if (resp != RT_NULL && resp_len > 0)
    {
        err = rt_sem_take(uart_rx_sem, rt_tick_from_millisecond(timeout_ms));
        if (err != RT_EOK)
        {
            LOG_W("AT command timeout");
            return -1;
        }

        /* 读取响应 */
        read_len = rt_device_read(mqtt_client.uart_dev, 0, resp, resp_len - 1);
        if (read_len > 0)
        {
            resp[read_len] = '\0';
            LOG_D("AT RESP: %s", resp);
            return 0;
        }
    }

    return 0;
}

/**
 * WiFi连接检查
 */
static rt_bool_t check_wifi_connected(void)
{
    char resp[128] = {0};

    /* 发送AT+CWJAP?命令检查WiFi连接状态 */
    if (at_send_command("AT+CWJAP?\r\n", resp, sizeof(resp), 2000) == 0)
    {
        if (rt_strstr(resp, "+CWJAP:") != RT_NULL)
        {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

/**
 * 连接WiFi
 */
static int connect_wifi(const char *ssid, const char *password)
{
    char cmd[128] = {0};
    char resp[128] = {0};

    LOG_I("Connecting to WiFi: %s", ssid);

    /* 发送连接WiFi命令 */
    rt_snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    if (at_send_command(cmd, resp, sizeof(resp), 10000) != 0)
    {
        LOG_E("Failed to connect WiFi");
        return -1;
    }

    if (rt_strstr(resp, "OK") != RT_NULL)
    {
        LOG_I("WiFi connected successfully");
        mqtt_client.status = MQTT_STATUS_CONNECTING;
        return 0;
    }

    LOG_E("WiFi connection failed");
    return -1;
}

/**
 * 连接MQTT broker
 */
static int connect_mqtt_broker(void)
{
    char cmd[256] = {0};
    char resp[128] = {0};

    LOG_I("Connecting to MQTT broker: %s:%d", 
          mqtt_client.config.mqtt_broker, 
          mqtt_client.config.mqtt_port);

    /* 建立TCP连接 */
    rt_snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", 
                mqtt_client.config.mqtt_broker,
                mqtt_client.config.mqtt_port);

    if (at_send_command(cmd, resp, sizeof(resp), 5000) != 0)
    {
        LOG_E("Failed to connect MQTT broker");
        return -1;
    }

    if (rt_strstr(resp, "OK") != RT_NULL || rt_strstr(resp, "ALREADY") != RT_NULL)
    {
        LOG_I("TCP connection established");
        mqtt_client.status = MQTT_STATUS_CONNECTED;
        return 0;
    }

    return -1;
}

/**
 * 断开WiFi连接
 */
static void disconnect_wifi(void)
{
    char resp[64] = {0};

    LOG_W("Disconnecting WiFi");

    at_send_command("AT+CWQAP\r\n", resp, sizeof(resp), 2000);

    mqtt_client.status = MQTT_STATUS_DISCONNECTED;
}

/**
 * MQTT监控线程 - 检查WiFi连接状态
 */
static void wifi_monitor_thread_entry(void *param)
{
    rt_uint32_t reconnect_delay = 5;

    LOG_I("WiFi monitor thread started");

    while (1)
    {
        rt_thread_mdelay(5000);  /* 每5秒检查一次 */

        if (!check_wifi_connected())
        {
            LOG_W("WiFi disconnected, attempting to reconnect");

            mqtt_client.status = MQTT_STATUS_RECONNECTING;
            mqtt_client.reconnect_count++;

            /* 尝试重新连接 */
            for (int i = 0; i < 3; i++)
            {
                if (connect_wifi(mqtt_client.config.wifi_ssid, 
                               mqtt_client.config.wifi_password) == 0)
                {
                    LOG_I("WiFi reconnected successfully");
                    break;
                }

                rt_thread_mdelay(reconnect_delay * 1000);
                reconnect_delay = (reconnect_delay < 30) ? (reconnect_delay * 2) : 30;
            }

            if (!check_wifi_connected())
            {
                LOG_E("Failed to reconnect WiFi after retries");
                /* 触发SD卡备份事件 */
                if (mqtt_event != RT_NULL)
                {
                    rt_event_send(mqtt_event, MQTT_EVENT_DISCONNECTED);
                }
            }
        }
    }
}

/**
 * MQTT处理线程
 */
static void mqtt_thread_entry(void *param)
{
    LOG_I("MQTT thread started");

    /* 初始化UART设备 */
    mqtt_client.uart_dev = rt_device_find("uart2");
    if (mqtt_client.uart_dev == RT_NULL)
    {
        LOG_E("UART device not found, using uart1 as fallback");
        mqtt_client.uart_dev = rt_device_find("uart1");
        if (mqtt_client.uart_dev == RT_NULL)
        {
            LOG_E("Failed to find UART device");
            return;
        }
    }

    /* 打开UART设备 */
    if (rt_device_open(mqtt_client.uart_dev, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        LOG_E("Failed to open UART device");
        return;
    }

    /* 设置接收回调 */
    rt_device_set_rx_indicate(mqtt_client.uart_dev, uart_rx_callback);

    /* 初始化WiFi模块 */
    char resp[128] = {0};
    LOG_I("Initializing ESP8266 WiFi module");

    at_send_command("AT\r\n", resp, sizeof(resp), 2000);
    rt_thread_mdelay(100);

    /* 设置WiFi模式为station */
    at_send_command("AT+CWMODE=1\r\n", resp, sizeof(resp), 2000);
    rt_thread_mdelay(100);

    /* 连接WiFi */
    if (connect_wifi(mqtt_client.config.wifi_ssid, 
                    mqtt_client.config.wifi_password) != 0)
    {
        LOG_E("Initial WiFi connection failed");
        return;
    }

    rt_thread_mdelay(2000);

    /* 连接MQTT broker */
    if (connect_mqtt_broker() != 0)
    {
        LOG_E("Initial MQTT connection failed");
        return;
    }

    LOG_I("MQTT client ready");

    /* 主循环 */
    while (1)
    {
        rt_thread_mdelay(1000);

        /* 检查WiFi连接状态 */
        if (!check_wifi_connected())
        {
            mqtt_client.status = MQTT_STATUS_RECONNECTING;
        }
    }
}

/**
 * 初始化MQTT客户端
 */
int mqtt_client_init(mqtt_config_t *config)
{
    rt_err_t err;

    if (config == RT_NULL)
    {
        LOG_E("MQTT config is NULL");
        return -1;
    }

    LOG_I("Initializing MQTT client");

    /* 复制配置 */
    rt_memcpy(&mqtt_client.config, config, sizeof(mqtt_config_t));
    mqtt_client.status = MQTT_STATUS_DISCONNECTED;
    mqtt_client.reconnect_count = 0;

    /* 创建信号量 */
    if (uart_rx_sem == RT_NULL)
    {
        uart_rx_sem = rt_sem_create("uart_rx_sem", 0, RT_IPC_FLAG_FIFO);
        if (uart_rx_sem == RT_NULL)
        {
            LOG_E("Failed to create UART RX semaphore");
            return -1;
        }
    }

    /* 创建事件 */
    if (mqtt_event == RT_NULL)
    {
        mqtt_event = rt_event_create("mqtt_event", RT_IPC_FLAG_FIFO);
        if (mqtt_event == RT_NULL)
        {
            LOG_E("Failed to create MQTT event");
            return -1;
        }
    }

    LOG_I("MQTT client initialized successfully");
    return 0;
}

/**
 * 启动MQTT服务
 */
int mqtt_client_start(void)
{
    rt_err_t err;

    if (mqtt_client.mqtt_thread != RT_NULL)
    {
        LOG_W("MQTT thread already started");
        return 0;
    }

    /* 创建MQTT处理线程 */
    mqtt_client.mqtt_thread = rt_thread_create(
        "mqtt_thread",
        mqtt_thread_entry,
        RT_NULL,
        MQTT_THREAD_STACK_SIZE,
        MQTT_THREAD_PRIORITY,
        10
    );

    if (mqtt_client.mqtt_thread == RT_NULL)
    {
        LOG_E("Failed to create MQTT thread");
        return -1;
    }

    err = rt_thread_startup(mqtt_client.mqtt_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start MQTT thread: %d", err);
        return -1;
    }

    /* 创建WiFi监控线程 */
    mqtt_client.wifi_monitor_thread = rt_thread_create(
        "wifi_monitor",
        wifi_monitor_thread_entry,
        RT_NULL,
        WIFI_MONITOR_STACK_SIZE,
        MQTT_THREAD_PRIORITY + 1,
        10
    );

    if (mqtt_client.wifi_monitor_thread == RT_NULL)
    {
        LOG_E("Failed to create WiFi monitor thread");
        return -1;
    }

    err = rt_thread_startup(mqtt_client.wifi_monitor_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start WiFi monitor thread: %d", err);
        return -1;
    }

    LOG_I("MQTT service started successfully");
    return 0;
}

/**
 * 发布MQTT消息
 */
int mqtt_client_publish(const char *topic, const void *payload, rt_size_t payload_len)
{
    char cmd[512] = {0};
    char resp[128] = {0};

    if (topic == RT_NULL || payload == RT_NULL)
    {
        return -1;
    }

    if (mqtt_client.status != MQTT_STATUS_CONNECTED)
    {
        LOG_W("MQTT not connected");
        return -1;
    }

    /* 这是一个简化的实现，实际使用需要遵循MQTT协议 */
    LOG_D("MQTT publish to [%s]: %.*s", topic, payload_len, (char *)payload);

    return 0;
}

/**
 * 发送CAN数据到MQTT
 */
int mqtt_client_publish_can_data(rt_uint32_t can_id, const rt_uint8_t *data, rt_uint8_t len)
{
    char payload[256] = {0};
    char data_str[16] = {0};
    int offset = 0;
    int i;

    if (data == RT_NULL || len == 0)
    {
        return -1;
    }

    if (mqtt_client.status != MQTT_STATUS_CONNECTED)
    {
        LOG_W("MQTT not connected, cannot publish CAN data");
        return -1;
    }

    /* 构造JSON格式的载荷 */
    offset += rt_snprintf(payload + offset, sizeof(payload) - offset, 
                         "{\"can_id\":\"0x%03X\",\"data\":[", can_id);

    for (i = 0; i < len; i++)
    {
        if (i > 0)
        {
            offset += rt_snprintf(payload + offset, sizeof(payload) - offset, ",");
        }
        offset += rt_snprintf(payload + offset, sizeof(payload) - offset, "0x%02X", data[i]);
    }

    offset += rt_snprintf(payload + offset, sizeof(payload) - offset, "]}");

    LOG_D("CAN Data: %s", payload);

    /* 发布到MQTT */
    return mqtt_client_publish(mqtt_client.config.mqtt_publish_topic, payload, offset);
}

/**
 * 获取MQTT连接状态
 */
mqtt_status_t mqtt_client_get_status(void)
{
    return mqtt_client.status;
}

/**
 * 检查是否连接到WiFi
 */
rt_bool_t mqtt_client_is_wifi_connected(void)
{
    return check_wifi_connected();
}

/**
 * 停止MQTT服务
 */
int mqtt_client_stop(void)
{
    if (mqtt_client.uart_dev != RT_NULL)
    {
        rt_device_close(mqtt_client.uart_dev);
        mqtt_client.uart_dev = RT_NULL;
    }

    mqtt_client.status = MQTT_STATUS_DISCONNECTED;

    LOG_I("MQTT service stopped");
    return 0;
}
