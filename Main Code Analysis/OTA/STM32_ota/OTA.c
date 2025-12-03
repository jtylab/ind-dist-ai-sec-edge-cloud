#include "OTA.h"
#include "OLED.h"
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "ESP8266.h"
#include "crc.h"

const uint32_t eachdowload=512;
uint32_t fileSize=1024;
//ESP826.c
extern const uint32_t buff2_len;

struct VersionInfo vi;

uint32_t crc32_total=0;

//bootloader��app2������app1ʹ�õĻ���
uint8_t cBuffer[1024];


APP_FUNC jump2app;

uint32_t OTA_Calculate_CRC32(uint32_t *pData, uint32_t length)
{
    // ����CRC������
    __HAL_CRC_DR_RESET(&hcrc);
    // ���㲢����CRCֵ
    return HAL_CRC_Calculate(&hcrc, pData, length);
}


void OTA_Download_New_App(void)
{
	printf("----------download---begin----\n");
	OTA_Earse_Flash_APP2();//����ճ���
	char cmd[100]={0};
	char bin_url[200]={0};
	int rst;
	uint32_t step=0;//0
	while(1){
		printf("----------------------------------------------------new--AT+CIPSEND---%d\n",step);
		memset(cmd,0,100);
		memset(bin_url,0,200);
		sprintf(bin_url,"GET %s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%d-%d\r\nConnection: close\r\n\r\n",
		AT_PATH_APP_BIN,AT_HOST,step,step+eachdowload-1);
		step+=eachdowload;
		sprintf(cmd, "AT+CIPSEND=%d\r\n",strlen(bin_url));
		//printf("---------cmd--------%s\n",cmd);
		rst=ESP_Send_Cmd(cmd, "OK", 5000);
		if(rst){
		printf("cmd success\n");
		}else{
			printf("cmd failed\n");
		}
		//printf("---------bin_url----%s\n",bin_url);
		rst=ESP_Send_Cmd(bin_url, "fName", 5000);
		if(rst){
			printf("url success\n");
		}else{
			printf("url failed\n");
		}
		printf("\nstep:%d  fileSize:%d \n",step,fileSize);
		if(step>fileSize){
			OLED_Clear_Line(2);
			OLED_ShowString(2, 1,"download success");
			break;
		}
	}
	//CRC-32/MPEG-2
	//����У��
	//Ӳ��CRC CRC-16��ƽ��������ɿ��ԣ�  STM32C8T6 ��Ӳ�� CRC Ĭ���� CRC-32/MPEG-2���޷�ת��
	uint32_t crc32;
	crc32=OTA_Calculate_CRC32((uint32_t *)APP2ADDR,fileSize/4);
	printf("-crc32 total check--: 0x%08x  0x%08x  %s\n", crc32,crc32_total,crc32==crc32_total?"success":"failed");//0xafd86a9e  18388
	//���±�־λ
	OTA_Set_Version_Flag(VERSION_STATE_UPDATE);
	printf("none:0 update:1 done:2 version flag: %d\n",OTA_Get_Version_Flag());
	
	OLED_Clear_Line(4);
	OLED_ShowString(4, 1,crc32==crc32_total?"total CRC OK":"total CRC failed");
	osDelay(5000);
	//����
	HAL_NVIC_SystemReset();
}


uint8_t OTA_Read_Byte_From_Flash(uint32_t address) {
    uint8_t data = 0;
    // ����Flash�������Ҫ��
    HAL_FLASH_Unlock();
    // ֱ�Ӷ�ȡ
    data = *(__IO uint8_t*)address;
    // ��������Flash
    HAL_FLASH_Lock();
    return data;
}

void OTA_Download_Buffer_Hander(uint8_t *buff2)
{
				uint32_t i=0;
				while(memcmp(&buff2[i], "\r\n\r\n", 4) != 0  && i<=buff2_len){
					i++;
				}
				if(i<buff2_len){ //����\r\n\r\n
						//1 ������Ϣͷ
						//Content-Range: bytes12288-12579/12580 
						//Content-Length: 292
						uint8_t httphead[1024]={0};
						strncpy((char *)httphead, (char *)buff2, i);
						httphead[i] = '\0';
						uint32_t start=0;
						uint32_t end=0;
						uint32_t total=0;
						uint32_t length=0;
						uint32_t crc32_remote=0;
					
						char *line;
						// ��һ�ε���strtok������ԭʼ�ַ���
						line = strtok((char *)httphead, "\r\n");
						while (line != NULL) {
								if(strstr((char *)line,"Content-Range")!=NULL){
									//Content-Range: bytes12288-12579/12580 
									sscanf(line, "Content-Range: bytes%d-%d/%d", &start, &end,&total);
								}else if(strstr((char *)line,"Content-Length")!=NULL){
									//Content-Length: 292
									sscanf(line, "Content-Length: %d", &length);
								}else if(strstr((char *)line,"CRC32_part")!=NULL){
									sscanf(line, "CRC32_part: 0x%08x", &crc32_remote);
								}else if(strstr((char *)line,"CRC32_total")!=NULL){
									sscanf(line, "CRC32_total: 0x%08x", &crc32_total);
								}
								line = strtok(NULL, "\r\n");
						}
						printf("\n----------------------------------------------------http header // start:%d ,end:%d ,total:%d ,length:%d\n",start,end,total,length);	
						printf("\n----------------------------------------------------http header // crc32_remote:0x%08x, crc32_total:0x%08x \n",crc32_remote,crc32_total);	
						if(length>0){//��Ϣͷ����������
							fileSize=total;
							uint32_t batch=(start+eachdowload)/eachdowload;//�ڼ����Σ�������end���㣬���һ�β�׼
							//2 ������Ϣ��\r\n\r\n������Ϣ�忪ʼ��
							printf("----------------------------------------------------http body %d\n",batch);
							OLED_Clear_Line(3);
							char speed[50];
							sprintf(speed, "progress:%d/%d", batch, total%eachdowload==0?(total/eachdowload):(total/eachdowload+1));
							OLED_ShowString(3, 1,speed);
							//д��app2
							OTA_Write_Flash(APP2ADDR+start,buff2+(i+4),length);
							uint32_t crc32_local;
							crc32_local=OTA_Calculate_CRC32((uint32_t *)(APP2ADDR+start),length/4);//���һ��
							char crc32_check_result[30];
							sprintf(crc32_check_result, "part%d CRC %s",batch,crc32_local==crc32_remote?"OK":"FF");
							printf("\n%s\n",crc32_check_result);
							OLED_Clear_Line(4);
							OLED_ShowString(4, 1,crc32_check_result);
						}else{
							printf("http body failed \n%s\n",buff2);
						}
				}
}
void OTA_Earse_Flash_APP2()
{
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SectorError = 0;
	
	printf("[OTA] Erasing APP2 (Sector 16-22)...\n");
	
	HAL_FLASH_Unlock();
	
	// F407VET6: APP2��Sector 16-22 (7��Sector �� 64KB = 448KB)
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.Sector = 16;                   // ��ʼSector 16
	EraseInitStruct.NbSectors = 7;                 // Ҫ������Sector��
	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
		printf("[OTA] Erase APP2 success\n");
	} else {
		printf("[OTA] Erase APP2 failed: %u\n", SectorError);
	}
	
	HAL_FLASH_Lock();
}


uint8_t OTA_Erase_Page(uint32_t address)
{
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SectorError = 0;
	uint32_t sector = 0;
	
	printf("[OTA] Erasing sector at 0x%08x\n", address);
	
	// F407VET6 Sector����
	if (address >= 0x08000000 && address < 0x08010000) {
		sector = (address - 0x08000000) / 0x4000;  // Sector 0-3 (16KB)
	} else if (address >= 0x08010000 && address < 0x08020000) {
		sector = 4;  // Sector 4 (64KB)
	} else if (address >= 0x08020000 && address < 0x08080000) {
		sector = 5 + (address - 0x08020000) / 0x10000;  // Sector 5-15 (64KB)
	} else if (address >= 0x08080000 && address < 0x080F0000) {
		sector = 16 + (address - 0x08080000) / 0x10000;  // Sector 16-22 (64KB)
	} else if (address >= 0x080F0000 && address < 0x08100000) {
		sector = 23;  // Sector 23 (64KB)
	} else {
		printf("[OTA] Invalid address: 0x%08x\n", address);
		return 0;
	}
	
	HAL_FLASH_Unlock();
	
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.Sector = sector;
	EraseInitStruct.NbSectors = 1;
	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
		printf("[OTA] Erase sector success\n");
		HAL_FLASH_Lock();
		return 1;
	} else {
		printf("[OTA] Erase sector failed\n");
		HAL_FLASH_Lock();
		return 0;
	}
}


void OTA_Set_Version_Flag(Version_State_TypeDef state){
		if(!OTA_Erase_Page(UPFLAG))return;
		HAL_FLASH_Unlock(); //����Flash
		HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, UPFLAG, state);
		if (status == HAL_OK) {
			// д��ɹ�
			uint32_t ReadData;
			ReadData = *(uint32_t*)UPFLAG;  // ֱ�Ӷ�ȡ32λ����
			printf("write %d success\n",ReadData);
		} else {
			//д��ʧ�ܴ���
			printf("write failed\n");
		}
		HAL_FLASH_Lock(); // ����Flash
}



//���ͻ�ȡ�汾��Ϣָ��
void OTA_Get_VI_Info(void){
	char cmd[500]={0};
	char info_url[200]={0};
	int rst;
	//char *geturl="GET /file/info.txt HTTP/1.1\r\nHost: file.sa1.tunnelfrp.com\r\nConnection: close\r\n\r\n";
	sprintf(info_url,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",AT_PATH_INFO,AT_HOST);
	sprintf(cmd, "AT+CIPSEND=%d\r\n",strlen(info_url));
	rst=ESP_Send_Cmd(cmd, "OK", 3000);
	if(rst){
			printf("AT+CIPSEND success\n");
	}else{
			printf("AT+CIPSEND failed\n");
	}
	rst=ESP_Send_Cmd(info_url, "OK", 10000);
	if(rst){
			printf("AT+CONTENT success\n");
	}else{
			printf("AT+CONTENT failed\n");
	}
}
//�����汾��Ϣ
void OTA_VI_Buffer_Hander(const uint8_t *buff2,struct VersionInfo *vi){
	uint32_t i=0;
	while(memcmp(&buff2[i], "\r\n\r\n", 4) != 0  && i<=buff2_len){
			i++;
	}
	if(i<buff2_len){
						//1 ������Ϣͷ
						//Content-Length: 292
						uint8_t httphead[1024]={0};
						strncpy((char *)httphead, (char *)buff2, i);
						httphead[i] = '\0';
						int32_t length=-1;
						char *line;
						// ��һ�ε���strtok������ԭʼ�ַ���
						line = strtok((char *)httphead, "\r\n");
						while (line != NULL) {
								if(strstr((char *)line,"Content-Length")!=NULL){
									//Content-Length: 292
									sscanf(line, "Content-Length: %d", &length);
								}
								line = strtok(NULL, "\r\n");
						}
						
						if(length>=0){
							memset(httphead,0,1024);
							memcpy(httphead, buff2 + i+4, length);
							//printf("\njson :%s \n",httphead);
							char version[50], info[50], url[100];
							if(sscanf((char *)httphead, 
								"{\"version\":\"%49[^\"]\", \"url\":\"%99[^\"]\", \"info\":\"%49[^\"]\"}", 
									version, url, info) == 3
									)
							{
									strcpy(vi->version,version);
									strcpy(vi->url,url);
									strcpy(vi->info,info);
									vi->flag=1;//�������
									return;
							} else {
									strcpy(vi->version,MCU_VER);
									printf("Failed to parse JSON\n");
							}
						}
	}
	strcpy(vi->version,MCU_VER);
	vi->flag=1;//�������
}

uint8_t OTA_Version_Compare(char *version)
{
	uint32_t v1,v2,v3,m1,m2,m3;
	sscanf(version,"%d.%d.%d",&v1,&v2,&v3);
	sscanf(MCU_VER,"%d.%d.%d",&m1,&m2,&m3);
	if(v1>m1)return 1;
	if(v2>m2)return 1;
	if(v3>m3)return 1;
	return 0;
}


void OTA_Bootloader_Copy(uint32_t SourceAddress, uint32_t TargetAddress)
{
	printf("[OTA] Copying APP2 to APP1...\n");
	
	uint32_t copy_size = 0x70000;      // 448KB (APP2��С)
	uint32_t chunk_size = 0x1000;      // ÿ��4KB
	uint32_t offset = 0;
	uint8_t buffer[0x1000];
	
	while (offset < copy_size) {
		uint32_t current_chunk = (offset + chunk_size > copy_size) ? 
		                         (copy_size - offset) : chunk_size;
		
		memcpy(buffer, (void *)(SourceAddress + offset), current_chunk);
		OTA_Write_Flash(TargetAddress + offset, buffer, current_chunk);
		
		offset += current_chunk;
		
		if (offset % 0x10000 == 0) {  // ÿ64KB��ӡ����
			printf("[OTA] Copied %u KB\n", offset / 1024);
		}
	}
	
	printf("[OTA] Copy completed\n");
}

Version_State_TypeDef OTA_Get_Version_Flag(void){
		uint32_t ReadData;
		ReadData = *(uint32_t*)UPFLAG;
		return (Version_State_TypeDef)ReadData;
}



void OTA_Read_Flash(uint32_t address,uint8_t *Nbuffer ,uint32_t length)
{
	uint32_t temp = 0;
	uint32_t count = 0;
	while(count < length)
	{
		temp = *((volatile uint32_t *)address);
		*Nbuffer++ = (temp & 0xff);
		count++;
    if(count >= length)
      break;
	  *Nbuffer++ = (temp >> 8) & 0xff;
		count++;
		if(count >= length)
      break;
		*Nbuffer++ = (temp >> 16) & 0xff;
		count++;
		if(count >= length)
      break;
		*Nbuffer++ = (temp >> 24) & 0xff;
		count++;
		if(count >= length)
      break;
		address += 4;
	}
}


void OTA_Write_Flash(uint32_t address,uint8_t *pBuffer,uint32_t Numlengh)
{
	uint32_t i ,temp;
  HAL_FLASH_Unlock();
	for(i = 0; i < Numlengh;i+= 4)
	{
		temp =  (uint32_t)pBuffer[i+3]<<24;
		temp |=  (uint32_t)pBuffer[i+2]<<16;
		temp |=  (uint32_t)pBuffer[i+1]<<8;
		temp |=  (uint32_t)pBuffer[i];
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address+i, temp);
	}
  HAL_FLASH_Lock();
}



void OTA_Device_Info(void){
	OLED_ShowString(1, 1, "F_SIZE:");	//��ʾ��̬�ַ���
	OLED_ShowHexNum(1, 8, *((__IO uint16_t *)(0x1FFFF7E0)), 4);		//ʹ��ָ���ȡָ����ַ�µ����������Ĵ���
	OLED_ShowString(2, 1, "U_ID:");		//��ʾ��̬�ַ���
	OLED_ShowHexNum(2, 6, *((__IO uint16_t *)(0x1FFFF7E8)), 4);		//ʹ��ָ���ȡָ����ַ�µĲ�ƷΨһ���ݱ�ʶ�Ĵ���
	OLED_ShowHexNum(2, 11, *((__IO uint16_t *)(0x1FFFF7E8 + 0x02)), 4);
	OLED_ShowHexNum(3, 1, *((__IO uint32_t *)(0x1FFFF7E8 + 0x04)), 8);
	OLED_ShowHexNum(4, 1, *((__IO uint32_t *)(0x1FFFF7E8 + 0x08)), 8);
}


void Jump2APP(uint32_t app_address)
{
	volatile uint32_t JumpAPPaddr;
	 if (((*(uint32_t*)app_address) &0x2FFE0000 ) == 0x20000000)
	 {
			JumpAPPaddr = *(volatile uint32_t*)(app_address + 4);
			jump2app = (APP_FUNC) JumpAPPaddr;
			printf("APP��ת��ַ��%x\r\n",app_address);
			__set_MSP(*(__IO uint32_t*) app_address);
			jump2app();
	 }
	 else{
			printf("�����ַ���Ϸ�\r\n");
	 }
}


