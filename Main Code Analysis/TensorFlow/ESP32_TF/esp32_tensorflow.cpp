

[env:esp32-s3-devkitm-1]
platform = espressif32
board = esp32-s3-devkitm-1
framework = arduino
lib_deps = 
	tanakamasayuki/TensorFlowLite_ESP32@^1.0.0




#include "sensor_handler.h"
#include "tf_lite_handler.h"
#include "can_handler.h"
#include "anomaly_model_data.h"




  // 初始化AI系统
  initializeAISystems();



  
  // AI推理循环 (每500ms一次)
  if (millis() - lastInferenceTime >= AI_INFERENCE_INTERVAL)
  {
    lastInferenceTime = millis();
    runAIInference();
  }


  
// ========== 初始化AI系统 ==========
void initializeAISystems()
{
  Serial.println("\n[系统] 初始化AI边缘计算模块...");
  
  // 初始化传感器
  Serial.println("[系统] 初始化传感器...");
  if (!sensorHandler.init())
  {
    Serial.println("[系统] ⚠️ 传感器初始化出现问题，但继续运行");
  }
  
  // 初始化TFLite模型
  Serial.println("[系统] 初始化TensorFlow Lite模型...");
  if (!anomalyDetector.init())
  {
    Serial.println("[系统] ❌ 模型初始化失败");
    LED_ON;
    return;
  }

}



  
// ========== AI推理主函数 ==========
void runAIInference()
{
  if (!anomalyDetector.isInitialized() || !canHandler.isInitialized())
  {
    return;
  }
  
  // 读取传感器数据
  SensorData_t sensor_data;
  bool sensor_ok = sensorHandler.readAllSensors(sensor_data);
  
  // 打印传感器数据（调试用）
  sensorHandler.printSensorData(sensor_data);
  
  // 规范化数据用于推理
  float input_data[6];
  sensorHandler.normalizeSensorData(sensor_data, input_data, 6);
  
  // 运行推理
  unsigned long inference_start = millis();
  float safety_score = anomalyDetector.runInference(input_data, 6);
  unsigned long inference_time = millis() - inference_start;
  
  if (safety_score < 0)
  {
    Serial.println("[AI] 推理失败");
    canHandler.sendError(1, "Inference Failed");
    return;
  }
  
  // 累计计数
  total_inference_count++;
  
  // 判断是否异常（安全系数低于阈值）
  const float ANOMALY_THRESHOLD = 0.5f;  // 可调整
  if (safety_score < ANOMALY_THRESHOLD)
  {
    anomaly_count++;
    Serial.print("[AI] ⚠️ 检测到异常！安全系数: ");
    Serial.println(safety_score);
    LED_ON;
    handleAnomalyDetection();
  }
  else
  {
    Serial.print("[AI] ✓ 正常，安全系数: ");
    Serial.println(safety_score);
    LED_OFF;
  }

  }