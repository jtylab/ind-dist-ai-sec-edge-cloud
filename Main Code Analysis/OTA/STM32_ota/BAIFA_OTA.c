#include "BAIFA_OTA.h"
#include "ESP8266.h"
#include "OTA.h"
#include "OLED.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "crc.h"

extern UART_HandleTypeDef huart2;
extern const uint32_t buff2_len;
extern uint8_t buff2[1024];

/* 静态变量 */
static char last_error[256] = {0};
static uint32_t download_progress = 0;
static Baifa_Download_Progress_Callback_t g_progress_callback = NULL;

/* 内部函数声明 */
static uint32_t Baifa_Calculate_CRC32(uint32_t *pData, uint32_t length);
static int Baifa_Build_Version_Check_Request(char *buf, uint32_t buf_size);
static int Baifa_Build_Download_Request(char *buf, uint32_t buf_size, uint32_t start_byte, uint32_t end_byte);

/**
 * @brief 初始化巴法云OTA模块
 */
int Baifa_OTA_Init(void) {
    printf("[BAIFA_OTA] Initializing BAIFA OTA module...\n");
    memset(last_error, 0, sizeof(last_error));
    download_progress = 0;
    return BAIFA_OTA_SUCCESS;
}

/**
 * @brief 生成巴法云认证签名
 */
void Baifa_OTA_Gen_Auth(char *buf, uint32_t buf_size) {
    if (!buf || buf_size < 50) return;
    
    // 简单认证模式：deviceId=xxx&apikey=xxx
    snprintf(buf, buf_size, "?deviceId=%s&apikey=%s", BAIFA_DEVICE_ID, BAIFA_API_KEY);
}

/**
 * @brief 硬件CRC32计算
 */
static uint32_t Baifa_Calculate_CRC32(uint32_t *pData, uint32_t length) {
    __HAL_CRC_DR_RESET(&hcrc);
    return HAL_CRC_Calculate(&hcrc, pData, length);
}

/**
 * @brief 版本比较 (简单版本号比较: major.minor.patch)
 */
uint8_t Baifa_OTA_Version_Compare(const char *new_version, const char *current_version) {
    uint32_t new_major, new_minor, new_patch;
    uint32_t curr_major, curr_minor, curr_patch;
    
    if (!new_version || !current_version) {
        snprintf(last_error, sizeof(last_error), "Invalid version string");
        return 0;
    }
    
    sscanf(new_version, "%u.%u.%u", &new_major, &new_minor, &new_patch);
    sscanf(current_version, "%u.%u.%u", &curr_major, &curr_minor, &curr_patch);
    
    // 比较版本大小
    if (new_major > curr_major) return 1;
    if (new_major == curr_major && new_minor > curr_minor) return 1;
    if (new_major == curr_major && new_minor == curr_minor && new_patch > curr_patch) return 1;
    
    return 0;
}

/**
 * @brief 构建版本检查请求
 */
static int Baifa_Build_Version_Check_Request(char *buf, uint32_t buf_size) {
    char auth[100] = {0};
    Baifa_OTA_Gen_Auth(auth, sizeof(auth));
    
    int len = snprintf(buf, buf_size,
        "GET %s%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: STM32-OTA/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        BAIFA_VERSION_API, auth, BAIFA_HOST);
    
    return len;
}

/**
 * @brief 构建固件下载请求
 */
static int Baifa_Build_Download_Request(char *buf, uint32_t buf_size, 
                                        uint32_t start_byte, uint32_t end_byte) {
    char auth[100] = {0};
    Baifa_OTA_Gen_Auth(auth, sizeof(auth));
    
    int len = snprintf(buf, buf_size,
        "GET %s/%s%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Range: bytes=%u-%u\r\n"
        "User-Agent: STM32-OTA/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        BAIFA_FW_DOWNLOAD_API, BAIFA_DEVICE_ID, auth, 
        BAIFA_HOST, start_byte, end_byte);
    
    return len;
}

/**
 * @brief 提取HTTP响应头中的字段值
 */
static int Baifa_Extract_Header_Value(const uint8_t *response, uint32_t response_len,
                                       const char *header_name, char *out_value, uint32_t out_size) {
    if (!response || !header_name || !out_value) return 0;
    
    char *line_start = (char *)response;
    char *line_end;
    char temp_line[256] = {0};
    
    while ((line_end = strstr(line_start, "\r\n")) != NULL) {
        uint32_t line_len = line_end - line_start;
        if (line_len >= sizeof(temp_line)) {
            line_start = line_end + 2;
            continue;
        }
        
        strncpy(temp_line, line_start, line_len);
        temp_line[line_len] = '\0';
        
        if (strstr(temp_line, header_name) != NULL) {
            // 找到了这个header，提取值
            char *value_start = strchr(temp_line, ':');
            if (value_start) {
                value_start++; // 跳过冒号
                while (*value_start == ' ') value_start++; // 跳过空格
                strncpy(out_value, value_start, out_size - 1);
                return 1;
            }
        }
        
        line_start = line_end + 2;
        if (line_start - (char *)response >= response_len) break;
    }
    
    return 0;
}

/**
 * @brief 解析巴法云版本检查响应
 */
int Baifa_OTA_Parse_Version_Response(const uint8_t *http_response, uint32_t response_len, 
                                      Baifa_Version_Info_t *version_info) {
    if (!http_response || !version_info) {
        snprintf(last_error, sizeof(last_error), "Invalid parameters");
        return BAIFA_OTA_PARSE_ERROR;
    }
    
    // 查找HTTP Body分隔符
    uint32_t i = 0;
    while (i < response_len - 3) {
        if (http_response[i] == '\r' && http_response[i+1] == '\n' &&
            http_response[i+2] == '\r' && http_response[i+3] == '\n') {
            break;
        }
        i++;
    }
    
    if (i >= response_len - 3) {
        snprintf(last_error, sizeof(last_error), "Cannot find HTTP body separator");
        return BAIFA_OTA_PARSE_ERROR;
    }
    
    // 获取Content-Length
    char content_length_str[20] = {0};
    if (!Baifa_Extract_Header_Value(http_response, response_len, "Content-Length", 
                                     content_length_str, sizeof(content_length_str))) {
        snprintf(last_error, sizeof(last_error), "Missing Content-Length header");
        return BAIFA_OTA_PARSE_ERROR;
    }
    
    uint32_t body_len = atoi(content_length_str);
    if (body_len == 0 || body_len > 512) {
        snprintf(last_error, sizeof(last_error), "Invalid Content-Length: %u", body_len);
        return BAIFA_OTA_PARSE_ERROR;
    }
    
    // 解析JSON body
    // 格式: {"version":"1.0.1","url":"http://...","size":12345,"crc32":"0xaabbccdd","desc":"..."}
    uint8_t json_body[512] = {0};
    memcpy(json_body, http_response + i + 4, body_len);
    
    char *version_str = strstr((char *)json_body, "\"version\"");
    char *url_str = strstr((char *)json_body, "\"url\"");
    char *size_str = strstr((char *)json_body, "\"size\"");
    char *crc_str = strstr((char *)json_body, "\"crc32\"");
    char *desc_str = strstr((char *)json_body, "\"desc\"");
    
    if (!version_str || !url_str) {
        snprintf(last_error, sizeof(last_error), "Missing required JSON fields");
        return BAIFA_OTA_PARSE_ERROR;
    }
    
    // 提取版本号
    memset(version_info->version, 0, sizeof(version_info->version));
    sscanf(version_str, "\"version\":\"%19[^\"]\"", version_info->version);
    
    // 提取URL
    memset(version_info->download_url, 0, sizeof(version_info->download_url));
    sscanf(url_str, "\"url\":\"%255[^\"]\"", version_info->download_url);
    
    // 提取文件大小
    if (size_str) {
        sscanf(size_str, "\"size\":%u", &version_info->file_size);
    }
    
    // 提取CRC32
    if (crc_str) {
        uint32_t crc32_val = 0;
        sscanf(crc_str, "\"crc32\":\"0x%x\"", &crc32_val);
        version_info->crc32 = crc32_val;
    }
    
    // 提取描述
    if (desc_str) {
        memset(version_info->desc, 0, sizeof(version_info->desc));
        sscanf(desc_str, "\"desc\":\"%199[^\"]\"", version_info->desc);
    }
    
    printf("[BAIFA_OTA] Parsed version: %s, size: %u, crc32: 0x%08x\n", 
           version_info->version, version_info->file_size, version_info->crc32);
    
    return BAIFA_OTA_SUCCESS;
}

/**
 * @brief 检查是否有新版本
 */
int Baifa_OTA_Check_Version(Baifa_Version_Info_t *version_info) {
    if (!version_info) {
        snprintf(last_error, sizeof(last_error), "Invalid version_info pointer");
        return BAIFA_OTA_NETWORK_ERROR;
    }
    
    printf("[BAIFA_OTA] Checking version from BAIFA...\n");
    OLED_Clear_Line(2);
    OLED_ShowString(2, 1, "Check version...");
    
    // 构建版本检查请求
    char request[512] = {0};
    Baifa_Build_Version_Check_Request(request, sizeof(request));
    
    printf("[BAIFA_OTA] Sending version check request...\n");
    printf("%s\n", request);
    
    // 发送HTTP请求
    char cmd[50] = {0};
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", strlen(request));
    
    if (!ESP_Send_Cmd(cmd, "OK", BAIFA_HTTP_TIMEOUT)) {
        snprintf(last_error, sizeof(last_error), "AT+CIPSEND timeout");
        return BAIFA_OTA_NETWORK_ERROR;
    }
    
    if (!ESP_Send_Cmd(request, "OK", BAIFA_HTTP_TIMEOUT)) {
        snprintf(last_error, sizeof(last_error), "Version check request timeout");
        return BAIFA_OTA_NETWORK_ERROR;
    }
    
    // 等待响应
    osDelay(2000);
    
    // 解析响应
    int result = Baifa_OTA_Parse_Version_Response(buff2, strlen((char *)buff2), version_info);
    
    if (result != BAIFA_OTA_SUCCESS) {
        printf("[BAIFA_OTA] Parse error: %s\n", last_error);
        return result;
    }
    
    // 比较版本
    if (Baifa_OTA_Version_Compare(version_info->version, CURRENT_FW_VERSION)) {
        printf("[BAIFA_OTA] New version found: %s\n", version_info->version);
        return BAIFA_OTA_SUCCESS;
    } else {
        snprintf(last_error, sizeof(last_error), "No new version available");
        return BAIFA_OTA_NO_UPDATE;
    }
}

/**
 * @brief 验证固件CRC32
 */
uint8_t Baifa_OTA_Verify_Firmware(uint32_t file_size, uint32_t expected_crc32) {
    printf("[BAIFA_OTA] Verifying firmware CRC32...\n");
    OLED_Clear_Line(4);
    OLED_ShowString(4, 1, "Verify CRC...");
    
    uint32_t calculated_crc32 = Baifa_Calculate_CRC32((uint32_t *)APP2ADDR, file_size / 4);
    
    printf("[BAIFA_OTA] Calculated CRC32: 0x%08x, Expected: 0x%08x\n", 
           calculated_crc32, expected_crc32);
    
    if (calculated_crc32 == expected_crc32) {
        printf("[BAIFA_OTA] CRC32 verification passed!\n");
        OLED_Clear_Line(4);
        OLED_ShowString(4, 1, "CRC OK");
        return 1;
    } else {
        printf("[BAIFA_OTA] CRC32 verification failed!\n");
        OLED_Clear_Line(4);
        OLED_ShowString(4, 1, "CRC FAILED");
        snprintf(last_error, sizeof(last_error), "CRC32 mismatch");
        return 0;
    }
}

/**
 * @brief 下载固件到APP2区域
 */
int Baifa_OTA_Download_Firmware(Baifa_Version_Info_t *version_info, 
                                 Baifa_Download_Progress_Callback_t progress_callback) {
    if (!version_info || version_info->file_size == 0) {
        snprintf(last_error, sizeof(last_error), "Invalid version_info");
        return BAIFA_OTA_NETWORK_ERROR;
    }
    
    printf("[BAIFA_OTA] Starting firmware download (%u bytes)...\n", version_info->file_size);
    OLED_Clear_Line(2);
    OLED_ShowString(2, 1, "Downloading...");
    
    // 清空APP2区域
    OTA_Earse_Flash_APP2();
    
    g_progress_callback = progress_callback;
    uint32_t downloaded = 0;
    uint32_t chunk_size = BAIFA_DOWNLOAD_CHUNK_SIZE;
    
    while (downloaded < version_info->file_size) {
        uint32_t start_byte = downloaded;
        uint32_t end_byte = (downloaded + chunk_size - 1 < version_info->file_size) ? 
                            (downloaded + chunk_size - 1) : (version_info->file_size - 1);
        
        // 构建下载请求
        char request[512] = {0};
        Baifa_Build_Download_Request(request, sizeof(request), start_byte, end_byte);
        
        printf("[BAIFA_OTA] Downloading bytes %u-%u...\n", start_byte, end_byte);
        
        // 发送HTTP请求
        char cmd[50] = {0};
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", strlen(request));
        
        if (!ESP_Send_Cmd(cmd, "OK", BAIFA_HTTP_TIMEOUT)) {
            snprintf(last_error, sizeof(last_error), "Download AT+CIPSEND timeout at %u", downloaded);
            return BAIFA_OTA_NETWORK_ERROR;
        }
        
        if (!ESP_Send_Cmd(request, "OK", BAIFA_DOWNLOAD_TIMEOUT)) {
            snprintf(last_error, sizeof(last_error), "Download request timeout at %u", downloaded);
            return BAIFA_OTA_DOWNLOAD_TIMEOUT;
        }
        
        // 等待接收数据
        osDelay(1000);
        
        // 处理接收到的数据 (buff2中包含HTTP响应)
        // 这里需要调用OTA_Download_Buffer_Hander来处理
        OTA_Download_Buffer_Hander(buff2);
        
        downloaded += chunk_size;
        
        // 进度回调
        if (progress_callback) {
            progress_callback(downloaded, version_info->file_size);
        }
        
        // OLED显示进度
        char progress[50];
        uint32_t percent = (downloaded * 100) / version_info->file_size;
        sprintf(progress, "DL: %u%%", percent);
        OLED_Clear_Line(3);
        OLED_ShowString(3, 1, progress);
        
        osDelay(100);
    }
    
    printf("[BAIFA_OTA] Firmware download completed!\n");
    return BAIFA_OTA_SUCCESS;
}

/**
 * @brief 启动完整OTA更新流程
 */
int Baifa_OTA_Start(Baifa_Download_Progress_Callback_t progress_callback) {
    printf("[BAIFA_OTA] Starting OTA update procedure...\n");
    
    // 第1步：检查版本
    Baifa_Version_Info_t version_info = {0};
    int check_result = Baifa_OTA_Check_Version(&version_info);
    
    if (check_result != BAIFA_OTA_SUCCESS) {
        printf("[BAIFA_OTA] Version check failed: %s\n", last_error);
        if (check_result == BAIFA_OTA_NO_UPDATE) {
            OLED_Clear_Line(2);
            OLED_ShowString(2, 1, "Latest version");
        }
        return check_result;
    }
    
    printf("[BAIFA_OTA] New version available: %s\n", version_info.version);
    OLED_Clear_Line(2);
    OLED_ShowString(2, 1, "New version found");
    osDelay(2000);
    
    // 第2步：下载固件
    int download_result = Baifa_OTA_Download_Firmware(&version_info, progress_callback);
    if (download_result != BAIFA_OTA_SUCCESS) {
        printf("[BAIFA_OTA] Download failed: %s\n", last_error);
        OLED_Clear_Line(2);
        OLED_ShowString(2, 1, "Download failed");
        return download_result;
    }
    
    // 第3步：验证CRC32
    if (!Baifa_OTA_Verify_Firmware(version_info.file_size, version_info.crc32)) {
        printf("[BAIFA_OTA] Verification failed!\n");
        return BAIFA_OTA_CRC_ERROR;
    }
    
    // 第4步：设置更新标志
    OTA_Set_Version_Flag(VERSION_STATE_UPDATE);
    printf("[BAIFA_OTA] Update flag set, system will reboot...\n");
    
    OLED_Clear_Line(2);
    OLED_ShowString(2, 1, "Reboot to update");
    osDelay(2000);
    
    // 重启系统
    HAL_NVIC_SystemReset();
    
    return BAIFA_OTA_SUCCESS;
}

/**
 * @brief 获取最后的错误信息
 */
const char* Baifa_OTA_Get_Last_Error(void) {
    return last_error[0] != '\0' ? last_error : "Unknown error";
}
