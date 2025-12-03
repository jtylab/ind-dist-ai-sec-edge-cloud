#include "ESP8266.h"
#include "OTA.h"
#include <string.h>
#include <stdio.h>
#include <OLED.h>
 
const uint32_t buff1_len=1024;
const uint32_t buff2_len=1024;
const uint32_t temp_len=1024;

uint8_t buff1[1024]={0};
uint8_t buff2[1024]={0};
uint8_t temp[1024]={0};

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;


extern struct VersionInfo vi;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
		if(huart->Instance==USART1)
		{
			HAL_UART_Transmit(&huart2, (uint8_t *)buff1, strlen((const char *)buff1), HAL_MAX_DELAY);
			memset(buff1,0,buff1_len);
			HAL_UARTEx_ReceiveToIdle_DMA(&huart1,buff1,buff1_len);
		}
		else if(huart->Instance==USART2)
		{
			if(strstr((char *)buff2,"version")!=NULL
						 &&strstr((char *)buff2,"url")!=NULL
						 &&strstr((char *)buff2,"info")!=NULL)
		  {
					OTA_VI_Buffer_Hander(buff2,&vi);
			}else if(strstr((char *)buff2,"IPD")!=NULL)
			{
					OTA_Download_Buffer_Hander(buff2);
			} 
			else
			{
				HAL_UART_Transmit(&huart1, (uint8_t *)buff2, strlen((const char *)buff2), HAL_MAX_DELAY);
			}
			if(strstr((char *)buff2,"IPD")!=NULL||
				 strstr((char *)buff2,"OK")!=NULL||
				 strstr((char *)buff2,"CIPMUX")!=NULL
			){
				memset(temp,0,temp_len);
			  memcpy(temp,buff2,temp_len);
			}
			memset(buff2,0,buff2_len);
			HAL_UARTEx_ReceiveToIdle_DMA(&huart2,buff2,buff2_len);
		}
}



int ESP_Send_Cmd(const char *cmd,const char *resp,uint32_t timeout){
	HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
	uint32_t tick=HAL_GetTick();
	while(HAL_GetTick()-tick<timeout){
		if(strstr((char *)temp,resp)!=NULL){
			memset(temp,0,temp_len);
			return 1;
		}
		osDelay(200);//���б�ɾ��
	}
	memset(temp,0,temp_len);
	return 0;
}




void ESP_Init(void){
	OLED_Clear_Line(2);
	OLED_ShowString(2, 1,"init WIFI...");
	
	memset(buff1,0,buff1_len);
	memset(buff2,0,buff2_len);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart2,buff2,buff2_len);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1,buff1,buff1_len);
	
	int rst;
	char AT_CMD[CMD_LEN]={0};
	
//	printf("----------------------------------------------AT---------------\n");
// // ����Ӳ�������ָ�����ʡ��
//	strcpy(AT_CMD, "AT\r\n");
//	rst=ESP_Send_Cmd("AT\r\n", "K", 2000);
//	if(rst){
//		printf("AT success\n");
//	}else{
//		printf("AT failed\n");
//	}

	
//	printf("----------------------------------------------CWMODE---------------\n");
//0: �� Wi-Fi ģʽ�����ҹر� Wi-Fi RF
//1: Station ģʽ
//2: SoftAP ģʽ
//3: SoftAP+Station ģʽ
	//��һ�����ú�Ĭ��Ϊ1
//	rst=ESP_Send_Cmd("AT+CWMODE=1\r\n", "OK", 2000);
//	if(rst){
//		printf("AT+CWMODE=1 success\n");
//	}else{
//		printf("AT+CWMODE=1 failed\n");
//	}
	
	printf("----------------------------------------------WIFI---------------\n");
	memset(AT_CMD,0,CMD_LEN);
	sprintf(AT_CMD,"AT+CWJAP=\"%s\",\"%s\"\r\n",AT_SSID,AT_PASSWORD);
	rst=ESP_Send_Cmd(AT_CMD, "CONNECTED", 10000);
	if(rst){
		printf("AT+CWJAP success\n");
	}else{
		printf("AT+CWJAP failed\n");
	}

//	printf("----------------------------------------------CIPMUX---------------\n");
//	0: ������
//	1: ������
//  Ĭ��ֵ��0 ��ˣ����ָ�����ʡ��
//	memset(AT_CMD,0,CMD_LEN);
//	sprintf(AT_CMD,"AT+CIPMUX=%d\r\n",0);
//	rst=ESP_Send_Cmd(AT_CMD, "CIPMUX", 3000);//������
//	if(rst){
//		printf("AT+CIPMUX success\n");
//	}else{
//		printf("AT+CIPMUX failed\n");
//	}

	printf("----------------------------------------------TCP---------------\n");
	memset(AT_CMD,0,CMD_LEN);
	sprintf(AT_CMD,"AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",AT_HOST,AT_PORT);
	rst=ESP_Send_Cmd(AT_CMD, "OK", 10000);
	OLED_Clear_Line(2);
	if(rst){
		printf("AT+CIPSTART HTTP success\n");
		OLED_ShowString(2, 1,"WIFI success");
	}else{
		printf("AT+CIPSTART HTTP failed\n");
		OLED_ShowString(2, 1,"WIFI failed");
	}
	
}

