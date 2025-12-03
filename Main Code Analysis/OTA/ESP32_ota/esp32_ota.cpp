#include <httpUpdate.h>
#include <HTTPClient.h>

#define Version 7

// ===== 系统配置 =====
// OTA版本检查
unsigned long lastCheckTime = 0;
const unsigned long OTA_CHECK_INTERVAL = 30000; // OTA检查间隔30秒


// 固件链接，在巴法云控制台复制、粘贴到这里即可
String upUrl = "http://bin.bemfa.com/b/3BcYTk0N2NiYTg1NmQyNDBmM2I0ZjFmZDg4MmExZjhmMDI=esp32s3.bin";

// 巴法云OTA配置
#define BEMFA_PRODUCT_KEY "your_product_key"
#define BEMFA_DEVICE_NAME "esp32s3"
#define BEMFA_DEVICE_SECRET "your_secret"


/**
 * 循环函数
 */
void loop()
{
  // OTA版本检查 (每30秒一次)
  if (WiFi.status() == WL_CONNECTED && millis() - lastCheckTime >= OTA_CHECK_INTERVAL)
  {
    lastCheckTime = millis();
    Serial.println("\n[OTA] 开始版本检查...");
    
    int latestVersion = getBemfaLatestVersion();
    if (latestVersion > Version)
    {
      Serial.println("[OTA] 发现新版本：" + String(latestVersion) + "（本地：" + String(Version) + "）");
      Serial.println("[OTA] 3秒后开始升级...");
      delay(3000);
      updateBin();
    }
    else if (latestVersion == Version)
    {
      Serial.println("[OTA] 当前已是最新版本");
    }
  }

  delay(10);
  }


  
// ========== 获取巴法云最新版本号 ==========
int getBemfaLatestVersion()
{
  const String bemfaCheckUrl = "https://cloud.bemfa.com/api/ota/check";
  HTTPClient http;
  int latestVersion = -1;

  // 构造POST请求参数
  String postData = "{\"topic\":\"ota/" + String(BEMFA_PRODUCT_KEY) + "/" + BEMFA_DEVICE_NAME + "\","
                    "\"token\":\"" + BEMFA_DEVICE_SECRET + "\","
                    "\"version\":\"" + String(Version) + "\"}";

  http.begin(bemfaCheckUrl);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(postData);
  if (httpCode == HTTP_CODE_OK)
  {
    String response = http.getString();
    Serial.println("[OTA] 响应: " + response);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (!error)
    {
      int code = doc["code"];
      if (code == 0)
      {
        bool hasNew = doc["data"]["has_new"];
        if (hasNew)
        {
          String versionStr = doc["data"]["version"];
          versionStr.replace("v", "");
          latestVersion = versionStr.toInt();
        }
        else
        {
          latestVersion = Version;
        }
      }
    }
    else
    {
      Serial.println("[OTA] JSON解析失败");
    }
  }
  else
  {
    Serial.println("[OTA] 请求失败，HTTP码：" + String(httpCode));
  }

  http.end();
  return latestVersion;
}



// ========== 升级回调函数 ==========
void update_started()
{
  Serial.println("[OTA] 升级进程已启动");
  LED_ON;
}

void update_finished()
{
  Serial.println("[OTA] 升级进程已完成");
  LED_OFF;
}

void update_progress(int cur, int total)
{
  Serial.printf("[OTA] 进度: %d / %d bytes\n", cur, total);
}

void update_error(int err)
{
  Serial.printf("[OTA] 错误代码: %d\n", err);
  LED_OFF;
}

/**
 * 固件升级函数
 */
void updateBin()
{
  Serial.println("[OTA] 开始升级");
  WiFiClient UpdateClient;

  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);

  t_httpUpdate_return ret = httpUpdate.update(UpdateClient, upUrl);
  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.println("[OTA] 升级失败");
    break;
  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("[OTA] 无需更新");
    break;
  case HTTP_UPDATE_OK:
    Serial.println("[OTA] 升级成功");
    break;
  }
}
