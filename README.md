### 项目名称:
工业级多节点分布式安全监测与AI边缘-云端协同系统
##
### 项目简介:
多节点分布式系统架构，子节点本地进行多个传感器数据的AI边缘计算（如果超过安全预警则立刻执行保护措施）。而后通过CAN总线将数据发送到主节点，主节点通过MQTT上传至云端（如果检测到无法连接云端，则把数据储存在SD卡中，待成功连上云端后通过HTPP再发送数据）。子节点的tensorflow AI检测模型可以通过OTA更新固件。子节点1使用ESP32-S3＋CAN收发器，基于FreeRTOS，搭载AI边缘检测，拥有温湿度传感器DHT11，振动传感器MPU6050，气体传感器MQ-9，通过本地wifi升级固件。子节点2使用STM32F407VET6+CAN收发器+esp32wifi模块，基于FreeRTOS，搭载AI边缘检测，拥有光电传感器E3F-DS30C4，振动传感器，通过本地wifi升级固件。主节点使用STM32F407ZGT6+ESP8266wifi模块+CAN收发器+OLED显示屏+SD卡，基于RTthread，MQTT协议，HTTP协议。wifi接受OTA固件升级。
##
### 文件说明:
mian
#    |—— "Complete code" —— 文件夹里附有完整工程的源码。
    |—— "Main Code Analysis" —— 主要代码文件
        |—— "CAN"
            |—— "ESP32_can"
            |—— "HAL_can"
            |—— "RTthread_can"
        |—— "MQTT"
        |—— "OTA"
            |—— "ESP32_ota"
            |—— "STM32_ota"
        |—— "SD"
        |—— "TensorFlow"
            |—— "ESP32_TF"
            |—— "HAL_TF"
        |—— "WiFi"
            |—— "ESP32_Wifi"
            |—— "HAL_Wifi"
            |—— "RTthread_Wifi"
##
### 相关网址:
##### 巴法云（远端固件传输） https://bemfa.com/
##### TensorFlow（机器学习源码） https://tensorflow.google.cn/?hl=zh-cn
##### ESP-IDF（乐鑫IDE官网） https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/get-started/index.html
##### Platformio（本项目ESP32-S3的编译环境） https://platformio.org/
##### RT-thread（本项目stm32f407vgt6的编译环境） https://www.rt-thread.org/
##### STM32CubeMX - Keil（本项目stm32f407vet6的编译环境） https://www.st.com/en/development-tools/stm32cubemx.html - https://www.keil.com/
##
### 注意事项:
   ##### 1.由于stm32的官方没有开源X-CUBE-AI的源码，因此stm32f407vet6的代码只能在keil里编译
   ##### 2.目前只有在arduino环境才能使用TensorFlow，推断是ESP-IDF的开发环境还没配置好 

##
### 项目实现细节:
本项目使用外接CAN收发器将不同的设备进行挂载CAN总线;两个esp作为stm32的wifi连接设备,通过AT指令实现通信;涉及Freertos和RT-thread两款rtos;".tflite"文件通过python的开源tensorflow库训练,esp32的ai边缘计算调用platformio中的"TensorFlowLite_ESP32"库通过python将".tflite"文件转换为C数组,stm32的ai边缘计算使用STM32CubeMX的X-CUBE-AI组件解析".tflite";stm32f407vgt6的SD,MQTT,AT通过RTThread的插件生成并调用;
##
## !!!详细的视频，图片等，请持续关注后续的更新.....

