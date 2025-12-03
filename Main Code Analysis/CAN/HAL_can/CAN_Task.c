
#include "CAN_Task.h"


extern float in_buffer[3];
extern float out_buffer[1];

__attribute__((aligned(4))) static uint8_t CAN_RxData[8];

static CAN_TxHeaderTypeDef CAN_TxHeader;
static CAN_RxHeaderTypeDef CAN_RxHeader;
CAN_TX_Data_t CAN_TX_Data;

osThreadId_t can_Task_ID;
osThreadAttr_t can_Task_attributes = {"can_task", 0, 0, 0, 0, 1024, (osPriority_t)osPriorityNormal7};


void can_task(void* argument){
	
	while(1){
		
		memcpy(&CAN_TX_Data.Raw_data[0],in_buffer,sizeof(in_buffer));
		CAN_TX_Data.Computing_data = out_buffer[0];
		
		BSP_CAN_SendData(0x123,(uint8_t *)&CAN_TX_Data,sizeof(CAN_TX_Data));
		osDelay(5);
	}
	
}


void can_task_Init(void){
	 can_Task_ID = osThreadNew(can_task, NULL, &can_Task_attributes);
}


void BSP_CAN_SendData(uint32_t ID, uint8_t* Data, uint32_t Len){
	uint32_t TxMailbox = 0;
	CAN_TxHeader.StdId = ID;
	CAN_TxHeader.IDE = CAN_ID_STD;
	CAN_TxHeader.RTR = CAN_RTR_DATA;
	CAN_TxHeader.DLC = Len;
	CAN_TxHeader.TransmitGlobalTime = DISABLE;
	
	uint8_t retry = 3;
	while (retry--) {
		// 步骤1：检查邮箱是否满，满则精准清理（仅中止“待发送”邮箱）
		if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
			for (uint32_t mb = 0; mb < 3; mb++) {
				// 替代原HAL_CAN_TXMAILBOX_STATE_PENDING：通过“待发送状态的数值特征”判断
				// 注：若HAL库无该枚举，可通过“尝试中止并检查返回值”间接判断
				HAL_CAN_AbortTxRequest(&hcan1, (1 << mb));
			}
			osDelay(1);  // 等待中止生效
		}

		// 步骤2：尝试发送数据
		if (HAL_CAN_AddTxMessage(&hcan1, &CAN_TxHeader, Data, &TxMailbox) == HAL_OK) {
			break;  // 发送成功，退出循环
		}
		osDelay(1);  // 重试前延时
	}

	// 步骤3：重试失败后，基于已定义的错误码判断致命性
	if (retry == 0) {
		uint32_t can_err = HAL_CAN_GetError(&hcan1);
		HAL_CAN_StateTypeDef can_state = HAL_CAN_GetState(&hcan1);

		// 致命错误判断（出现以下情况则停机）
		if (can_err & HAL_CAN_ERROR_BOF                 // 总线关闭
			|| can_err & HAL_CAN_ERROR_STF              // 填充错误
			|| can_err & HAL_CAN_ERROR_NOT_INITIALIZED  // 未初始化
			|| can_err & HAL_CAN_ERROR_NOT_READY        // 未准备好
			|| can_err & HAL_CAN_ERROR_PARAM            // 参数错误
			|| can_state == HAL_CAN_STATE_ERROR) {      // 外设错误状态（若有定义）
			Error_Handler();
		} else {

		}
	}
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan){
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CAN_RxHeader, CAN_RxData);
}
