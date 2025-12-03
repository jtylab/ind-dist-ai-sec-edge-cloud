#include <WiFi.h>


#define wifi_name "Mate 60" // WIFI名称，区分大小写，不要写错
#define wifi_password "12345678" // WIFI密码




  // 连接WiFi
  Serial.println("\n[WiFi] 正在连接...");
  WiFi.begin(wifi_name, wifi_password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20)
  {
    delay(500);
    Serial.print(".");
    LED_ON;
    delay(100);
    LED_OFF;
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] 连接成功");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n[WiFi] 连接失败，继续运行...");
  }