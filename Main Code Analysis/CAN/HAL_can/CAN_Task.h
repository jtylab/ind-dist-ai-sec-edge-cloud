
#ifndef CAN_Task_H
#define CAN_Task_H


#include "main.h"
#include "cmsis_os.h"
#include "can.h"
#include "task.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>

typedef struct CAN_TX_Data_t{
	float Raw_data[3];
	float Computing_data;
}CAN_TX_Data_t;

void can_task_Init(void);
void BSP_CAN_SendData(uint32_t ID, uint8_t* Data, uint32_t Len);




#endif