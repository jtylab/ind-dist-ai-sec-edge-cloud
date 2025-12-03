#include "can_handler.h"

 
  // 初始化CAN
  Serial.println("[系统] 初始化CAN总线...");
  if (!canHandler.init())
  {
    Serial.println("[系统] ❌ CAN初始化失败");
    return;
  }





  
  // 通过CAN发送数据
  // 1. 发送安全系数
  canHandler.sendSafetyScore(safety_score, sensor_data.timestamp);
  
  // 2. 发送传感器原始数据
  uint8_t sensor_status = sensor_ok ? 0x01 : 0x00;
  canHandler.sendSensorData(
    sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
    sensor_data.temperature, sensor_data.humidity, sensor_data.mq9_ppm,
    sensor_status
  );
  
  // 3. 发送模型状态（每10次推理发送一次）
  if (total_inference_count % 10 == 0)
  {
    canHandler.sendModelStatus(
      1,                          // 模型状态: 1=正常
      (uint8_t)inference_time,    // 推理时间
      AI_INFERENCE_INTERVAL,      // 处理周期
      anomaly_count,              // 异常计数
      total_inference_count       // 总计数
    );
   }