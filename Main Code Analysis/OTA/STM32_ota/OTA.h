#ifndef __OTA_H
#define __OTA_H

#include "stdint.h"
#include "cmsis_os.h"
#define MCU_VER "1.0.0"


#define ADDR_FLASH_BASE 0x08000000
#define ADDR_FLASH_1K   0x0400


// STM32F407VET6 Flashӳ�� (512KB)
#define APP1        0x20000      // APP1: 0x08020000 (448KB) - Sector 5-15
#define APP2        0x80000      // APP2: 0x08080000 (448KB) - Sector 16-22
#define UPFLAG1     0xF0000      // ��־λ: 0x080F0000 (64KB) - Sector 23

#define APP1ADDR    (ADDR_FLASH_BASE + APP1)     // 0x08020000
#define APP2ADDR    (ADDR_FLASH_BASE + APP2)     // 0x08080000
#define UPFLAG      (ADDR_FLASH_BASE + UPFLAG1)  // 0x080F0000

// Sector���� (����Flash����)
#define APP1_START_SECTOR  FLASH_SECTOR_5
#define APP1_END_SECTOR    FLASH_SECTOR_15
#define APP2_START_SECTOR  FLASH_SECTOR_16
#define APP2_END_SECTOR    FLASH_SECTOR_22
#define FLAG_SECTOR        FLASH_SECTOR_23



//������IP��ַ/����
//#define AT_HOST "xxx.xxx.com"


#define AT_HOST "file.sa1.tunnelfrp.com"
//�������˿�
#define AT_PORT "80"
//�汾��Ϣ�ļ�url·��
#define AT_PATH_INFO "/file/info.txt"
//�°�app��bin�ļ�url·��
#define AT_PATH_APP_BIN "/file/appled.bin"
//WIFI����
#define AT_SSID "21-4-9"
//WIFI����
#define AT_PASSWORD "guanyu409"


typedef void (*APP_FUNC)();

extern APP_FUNC jump2app;

typedef enum
{
  VERSION_STATE_NONE             = 0x00U,   /*!< no new version available      */
	VERSION_STATE_UPDATE,   									/*!< new version available         */
	VERSION_STATE_DONE,   										/*!< new version updated success   */
} Version_State_TypeDef;

struct VersionInfo {
    char version[10];  // ����ÿ���ַ�����󳤶�Ϊ99�����Ͻ�β��'\0'��
    char url[100];
    char info[100];
		uint8_t flag;
};


//----------------APP����------------------
//���ͷֶ�����ָ��
void OTA_Download_New_App(void);
//����ǰ�����App2
void OTA_Earse_Flash_APP2(void);

//�����ֶ����ط��ر���
void OTA_Download_Buffer_Hander(uint8_t *buff2);

//���ø��±�־λ
void OTA_Set_Version_Flag(Version_State_TypeDef state);
//��ҳ����,���ڸ��±�־λ֮ǰ
uint8_t OTA_Erase_Page(uint32_t addr);
//Ӳ��CRC32 У��
uint32_t OTA_Calculate_CRC32(uint32_t *pData, uint32_t length);

//���ͻ�ȡ�汾��Ϣָ��
void OTA_Get_VI_Info(void);
//�����汾��Ϣ
void OTA_VI_Buffer_Hander(const uint8_t *buff2,struct VersionInfo *vi);

uint8_t OTA_Version_Compare(char *version);

//----------------bootloader����------------------
//��ѯ���±�־λ
Version_State_TypeDef OTA_Get_Version_Flag(void);
//��app2���Ƶ�app1
void OTA_Bootloader_Copy(uint32_t SourceAddress,uint32_t TargetAddress);
//��ת��app1
void Jump2APP(uint32_t app_address);


//----------------APP��bootloader�й���------------------

void OTA_Write_Flash(uint32_t address,uint8_t *pBuffer,uint32_t Numlengh);

void OTA_Read_Flash(uint32_t address,uint8_t *Nbuffer ,uint32_t  length);

void OTA_Device_Info(void);

uint8_t OTA_Read_Byte_From_Flash(uint32_t address);

#endif
