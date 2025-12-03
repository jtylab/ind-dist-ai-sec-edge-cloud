
#include "Sensor_reading.h"
void AI_edge_computing(float buffer[], int in_len);

float in_buffer[3];
float out_buffer[1];

float Sensor_buffer[3];

osThreadId_t cSensor_reading_task_ID;
osThreadAttr_t Sensor_reading_task_attributes = {"Sensor_reading_task", 0, 0, 0, 0, 1024, (osPriority_t)osPriorityNormal7};

void Sensor_reading_task(void* argument){
	
	while(1){
		 
		
		
		//传感器1
		
		
		
		//传感器1
		
		//传感器2
		
		
		
		//传感器2
		
		//传感器3
		
		
		
		//传感器3
		
	
		
    AI_edge_computing(Sensor_buffer,sizeof(Sensor_buffer));
		osDelay(10);
	}
	
}

//ai边缘计算
void AI_edge_computing(float buffer[], int in_len){
	 for (int i = 0; i < in_len; i++) {
      	in_buffer[i] = buffer[i];
    }
	 	MX_X_CUBE_AI_Process();
}

void Sensor_reading_task_Init(void){
	 cSensor_reading_task_ID = osThreadNew(Sensor_reading_task, NULL, &Sensor_reading_task_attributes);
}
