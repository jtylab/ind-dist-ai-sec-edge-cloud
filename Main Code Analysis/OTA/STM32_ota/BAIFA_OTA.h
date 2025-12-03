#ifndef __BAIFA_OTA_H
#define __BAIFA_OTA_H

#include "stdint.h"
#include "baifa_config.h"
#include "cmsis_os.h"
/* 巴法云OTA状态码 */
typedef enum {
    BAIFA_OTA_SUCCESS = 0,           // 成功
    BAIFA_OTA_NETWORK_ERROR = -1,    // 网络错误
    BAIFA_OTA_PARSE_ERROR = -2,      // 解析错误
    BAIFA_OTA_NO_UPDATE = -3,        // 无新版本
    BAIFA_OTA_CRC_ERROR = -4,        // CRC校验失败
    BAIFA_OTA_DOWNLOAD_TIMEOUT = -5, // 下载超时
    BAIFA_OTA_FLASH_ERROR = -6,      // Flash操作错误
} Baifa_OTA_Status_t;

/* 版本信息结构体 */
typedef struct {
    char version[20];        // 版本号 如 "1.0.1"
    char download_url[256];  // 固件下载URL
    char desc[200];          // 更新描述
    uint32_t file_size;      // 文件大小（字节）
    uint32_t crc32;          // 文件CRC32校验值
    uint8_t force_update;    // 是否强制更新
} Baifa_Version_Info_t;

/* 下载进度回调函数指针 */
typedef void (*Baifa_Download_Progress_Callback_t)(uint32_t current, uint32_t total);

/* ============= 对外接口 ============= */

/**
 * @brief 初始化巴法云OTA模块
 * @return 初始化结果
 */
int Baifa_OTA_Init(void);

/**
 * @brief 检查是否有新版本
 * @param version_info 输出版本信息指针
 * @return 状态码
 */
int Baifa_OTA_Check_Version(Baifa_Version_Info_t *version_info);

/**
 * @brief 版本比较
 * @param new_version 新版本号字符串 如 "1.0.1"
 * @param current_version 当前版本号字符串 如 "1.0.0"
 * @return 1:需要升级 0:无需升级
 */
uint8_t Baifa_OTA_Version_Compare(const char *new_version, const char *current_version);

/**
 * @brief 下载新固件到Flash APP2区域
 * @param version_info 版本信息指针
 * @param progress_callback 进度回调函数（可为NULL）
 * @return 状态码
 */
int Baifa_OTA_Download_Firmware(Baifa_Version_Info_t *version_info, 
                                 Baifa_Download_Progress_Callback_t progress_callback);

/**
 * @brief 验证下载的固件的CRC32
 * @param file_size 固件大小
 * @param expected_crc32 期望的CRC32值
 * @return 1:校验成功 0:校验失败
 */
uint8_t Baifa_OTA_Verify_Firmware(uint32_t file_size, uint32_t expected_crc32);

/**
 * @brief 启动OTA更新流程（完整流程）
 * @param progress_callback 进度回调函数（可为NULL）
 * @return 状态码
 */
int Baifa_OTA_Start(Baifa_Download_Progress_Callback_t progress_callback);

/**
 * @brief 获取最后的错误信息
 * @return 错误描述字符串
 */
const char* Baifa_OTA_Get_Last_Error(void);

/* ============= 内部函数（可选导出） ============= */

/**
 * @brief 生成巴法云认证签名
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void Baifa_OTA_Gen_Auth(char *buf, uint32_t buf_size);

/**
 * @brief 解析巴法云版本检查响应
 * @param http_response HTTP响应数据
 * @param response_len 响应长度
 * @param version_info 输出版本信息
 * @return 解析结果
 */
int Baifa_OTA_Parse_Version_Response(const uint8_t *http_response, uint32_t response_len, 
                                      Baifa_Version_Info_t *version_info);

#endif /* __BAIFA_OTA_H */
