#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"
#include "cmsis_os.h"
#define CMD_LEN 100

//��ʼ��ESP8266
void ESP_Init(void);
//����ATָ��
int ESP_Send_Cmd(const char *cmd,const char *rst,uint32_t timeout);

#endif
