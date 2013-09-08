/**************************************************************************** * *   Copyright (C) 2012, 2013 TMR Development Team. All rights reserved. * *   Author : CHIA-CHENG, TSAO * *   E-Mail : chiacheng.tsao@gmail.com * *   Date :07/09/2013 * ****************************************************************************/#include <nuttx/config.h>#include <sys/types.h>#include <stdint.h>#include <stdbool.h>#include <stdlib.h>#include <unistd.h>#include <string.h>#include <errno.h>#include <debug.h>#include <nuttx/kmalloc.h>#include <nuttx/fs/ioctl.h>#include <nuttx/mtd.h>#include "systemlib/perf_counter.h"#include "internal_flash.h"/************************************************************************************ * Private Function Prototypes ************************************************************************************//* MTD driver methods */static int internal_flash_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks);static ssize_t internal_flash_bread(FAR struct mtd_dev_s *dev, off_t startblock,               size_t nblocks, FAR uint8_t *buf);static ssize_t internal_flash_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,                size_t nblocks, FAR const uint8_t *buf);static int internal_flash_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg);void internal_flash_test(void);/* Exported constants --------------------------------------------------------*//* Define the size of the sectors to be used */#define PAGE_SIZE               (uint32_t)0x4000  /* Page size = 16KByte *//* Device voltage range supposed to be [2.7V to 3.6V], the operation will    be done by word  */#define VOLTAGE_RANGE           (uint8_t)VoltageRange_3/* EEPROM start address in Flash */#define EEPROM_START_ADDRESS  ((uint32_t)0x08008000) /* EEPROM emulation start address:                                                  from sector2 : after 16KByte of used                                                   Flash memory *//* Pages 0 and 1 base and end addresses */#define PAGE0_BASE_ADDRESS    ((uint32_t)(EEPROM_START_ADDRESS + 0x0000))#define PAGE0_END_ADDRESS     ((uint32_t)(PAGE0_BASE_ADDRESS + (PAGE_SIZE - 1)))#define PAGE0_ID               FLASH_Sector_2#define PAGE1_BASE_ADDRESS    ((uint32_t)(PAGE0_BASE_ADDRESS + PAGE_SIZE))#define PAGE1_END_ADDRESS     ((uint32_t)(PAGE1_BASE_ADDRESS + (PAGE_SIZE - 1)))#define PAGE1_ID               FLASH_Sector_3/* Used Flash pages for EEPROM emulation */#define PAGE0                 ((uint16_t)0x0000)#define PAGE1                 ((uint16_t)0x0001)/* No valid page define */#define NO_VALID_PAGE         ((uint16_t)0x00AB)/* Page status definitions */#define ERASED                ((uint16_t)0xFFFF)     /* Page is empty */#define RECEIVE_DATA          ((uint16_t)0xEEEE)     /* Page is marked to receive data */#define VALID_PAGE            ((uint16_t)0x0000)     /* Page containing valid data *//* Valid pages in read and write defines */#define READ_FROM_VALID_PAGE  ((uint8_t)0x00)#define WRITE_IN_VALID_PAGE   ((uint8_t)0x01)/* Page full define */#define PAGE_FULL             ((uint8_t)0x80)/* Variables' number */#define NB_OF_VAR             ((uint8_t)0x03)uint16_t EE_Init(void);uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data);uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data);/* Global variable used to store variable value in read sequence */uint16_t DataVar = 0;static struct sector{    uint32_t    nsector;    unsigned    size;    uint32_t    base;} flash_sectors[] = {    { FLASH_Sector_0,   16 * 1024, 0x08000000},    { FLASH_Sector_1,   16 * 1024, 0x08004000},    { FLASH_Sector_2,   16 * 1024, 0x08008000},    { FLASH_Sector_3,   16 * 1024, 0x0800C000},    { FLASH_Sector_4,   64 * 1024, 0x08010000},    { FLASH_Sector_5,  128 * 1024, 0x08020000},    { FLASH_Sector_6,  128 * 1024, 0x08040000},    { FLASH_Sector_7,  128 * 1024, 0x08060000},    { FLASH_Sector_8,  128 * 1024, 0x08080000},    { FLASH_Sector_9,  128 * 1024, 0x080A0000},    { FLASH_Sector_10, 128 * 1024, 0x080C0000},    { FLASH_Sector_11, 128 * 1024, 0x080E0000}};/* Virtual address defined by the user: 0xFFFF value is prohibited */uint16_t VirtAddVarTab[NB_OF_VAR] = {0x5555, 0x6666, 0x7777};uint16_t VarDataTab[NB_OF_VAR] = {0, 0, 0};uint16_t VarValue = 0;/* Private function prototypes -----------------------------------------------*//* Private functions ---------------------------------------------------------*/static FLASH_Status EE_Format(void);static uint16_t EE_FindValidPage(uint8_t Operation);static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data);static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data);static uint16_t Check_Sector_Erased(uint32_t FLASH_Sector);/**  * @brief  Restore the pages to a known good state in case of page's status  *   corruption after a power loss.  * @param  None.  * @retval - Flash error code: on write Flash error  *         - FLASH_COMPLETE: on success  */uint16_t EE_Init(void){  uint16_t PageStatus0 = 6, PageStatus1 = 6;  uint16_t VarIdx = 0;  uint16_t EepromStatus = 0, ReadStatus = 0;  int16_t x = -1;  uint16_t  FlashStatus;  LOG(LOG_DEBUG, "EE_Init \n");  /* Get Page0 status */  PageStatus0 = (*(__IO uint16_t*)PAGE0_BASE_ADDRESS);  /* Get Page1 status */  PageStatus1 = (*(__IO uint16_t*)PAGE1_BASE_ADDRESS);  /* Check for invalid header states and repair if necessary */  switch (PageStatus0)  {    case ERASED:      LOG(LOG_DEBUG,"EE_Init >> ERASED \n");      if (PageStatus1 == VALID_PAGE) /* Page0 erased, Page1 valid */      {        /* Erase Page0 */        LOG(LOG_DEBUG,"EE_Init >> FLASH_EraseSector 0x%04X, 0x%04X \n", PAGE0_ID, VOLTAGE_RANGE);        if(0xFFFF != Check_Sector_Erased(PAGE0_ID))        {            LOG(LOG_DEBUG, "Page 0 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE0_ID,VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 0 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG,"EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else if (PageStatus1 == RECEIVE_DATA) /* Page0 erased, Page1 receive */      {        LOG(LOG_DEBUG, "EE_Init >> RECEIVE_DATA %d \n", FlashStatus);        /* Erase Page0 */        LOG(LOG_DEBUG, "EE_Init >> FLASH_EraseSector 0x%04X, 0x%04X \n", PAGE0_ID, VOLTAGE_RANGE);        if(0xFFFF != Check_Sector_Erased(PAGE0_ID))        {            LOG(LOG_DEBUG, "Page 0 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE0_ID, VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 0 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }        /* Mark Page1 as valid */        LOG(LOG_DEBUG, "EE_Init >> FLASH_ProgramHalfWord 0x%04X, 0x%04X \n", PAGE1_BASE_ADDRESS, VALID_PAGE);        FlashStatus = FLASH_ProgramHalfWord(PAGE1_BASE_ADDRESS, VALID_PAGE);        /* If program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else /* First EEPROM access (Page0&1 are erased) or invalid state -> format EEPROM */      {        /* Erase both Page0 and Page1 and set Page0 as valid page */        LOG(LOG_DEBUG, "EE_Init >> EE_Format \n");        FlashStatus = EE_Format();        /* If erase/program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      break;    case RECEIVE_DATA:      LOG(LOG_DEBUG, "EE_Init >> RECEIVE_DATA \n");      if (PageStatus1 == VALID_PAGE) /* Page0 receive, Page1 valid */      {        /* Transfer data from Page1 to Page0 */        for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++)        {          if (( *(__IO uint16_t*)(PAGE0_BASE_ADDRESS + 6)) == VirtAddVarTab[VarIdx])          {            x = VarIdx;          }          if (VarIdx != x)          {            /* Read the last variables' updates */            ReadStatus = EE_ReadVariable(VirtAddVarTab[VarIdx], &DataVar);            LOG(LOG_DEBUG, "EE_Init >> EE_ReadVariable 0x%04X, 0x%04X \n", VirtAddVarTab[VarIdx], DataVar);            /* In case variable corresponding to the virtual address was found */            if (ReadStatus != 0x1)            {              /* Transfer the variable to the Page0 */              LOG(LOG_DEBUG, "EE_Init >> EE_VerifyPageFullWriteVariable 0x%04X, 0x%04X \n", VirtAddVarTab[VarIdx], DataVar);              EepromStatus = EE_VerifyPageFullWriteVariable(VirtAddVarTab[VarIdx], DataVar);              /* If program operation was failed, a Flash error code is returned */              if (EepromStatus != FLASH_COMPLETE)              {                LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", EepromStatus);                return EepromStatus;              }            }          }        }        /* Mark Page0 as valid */        LOG(LOG_DEBUG, "EE_Init >> FLASH_ProgramHalfWord Page 0 \n");        FlashStatus = FLASH_ProgramHalfWord(PAGE0_BASE_ADDRESS, VALID_PAGE);        /* If program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }        /* Erase Page1 */        LOG(LOG_DEBUG, "EE_Init >> Erase Page 1 \n");        if(0xFFFF != Check_Sector_Erased(PAGE1_ID))        {            LOG(LOG_DEBUG, "Page 1 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE1_ID, VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 1 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else if (PageStatus1 == ERASED) /* Page0 receive, Page1 erased */      {        LOG(LOG_DEBUG, "EE_Init >> ERASED \n");        /* Erase Page1 */        LOG(LOG_DEBUG, "EE_Init >> Erase Page 1 \n");        if(0xFFFF != Check_Sector_Erased(PAGE1_ID))        {            LOG(LOG_DEBUG, "Page 1 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE1_ID, VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 1 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }        /* Mark Page0 as valid */        LOG(LOG_DEBUG, "EE_Init >> FLASH_ProgramHalfWord Page 0 \n");        FlashStatus = FLASH_ProgramHalfWord(PAGE0_BASE_ADDRESS, VALID_PAGE);        /* If program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else /* Invalid state -> format eeprom */      {        /* Erase both Page0 and Page1 and set Page0 as valid page */        FlashStatus = EE_Format();        /* If erase/program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      break;    case VALID_PAGE:      LOG(LOG_DEBUG, "EE_Init >> VALID_PAGE \n");      if (PageStatus1 == VALID_PAGE) /* Invalid state -> format eeprom */      {        /* Erase both Page0 and Page1 and set Page0 as valid page */        LOG(LOG_DEBUG, "EE_Init >> EE_Format \n");        FlashStatus = EE_Format();        /* If erase/program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else if (PageStatus1 == ERASED) /* Page0 valid, Page1 erased */      {        LOG(LOG_DEBUG, "EE_Init >> ERASED \n");        /* Erase Page1 */        LOG(LOG_DEBUG, "EE_Init >> FLASH_EraseSector 1 \n");        if(0xFFFF != Check_Sector_Erased(PAGE1_ID))        {            LOG(LOG_DEBUG, "Page 1 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE1_ID, VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 1 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      else /* Page0 valid, Page1 receive */      {        /* Transfer data from Page0 to Page1 */        for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++)        {          if ((*(__IO uint16_t*)(PAGE1_BASE_ADDRESS + 6)) == VirtAddVarTab[VarIdx])          {            x = VarIdx;          }          if (VarIdx != x)          {            /* Read the last variables' updates */            ReadStatus = EE_ReadVariable(VirtAddVarTab[VarIdx], &DataVar);            LOG(LOG_DEBUG, "EE_Init >> EE_ReadVariable 0x%04X, 0x%04X \n", VirtAddVarTab[VarIdx], DataVar);            /* In case variable corresponding to the virtual address was found */            if (ReadStatus != 0x1)            {              /* Transfer the variable to the Page1 */              LOG(LOG_DEBUG, "EE_Init >> EE_VerifyPageFullWriteVariable 0x%04X, 0x%04X \n", VirtAddVarTab[VarIdx], DataVar);              EepromStatus = EE_VerifyPageFullWriteVariable(VirtAddVarTab[VarIdx], DataVar);              /* If program operation was failed, a Flash error code is returned */              if (EepromStatus != FLASH_COMPLETE)              {                LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", EepromStatus);                return EepromStatus;              }            }          }        }        /* Mark Page1 as valid */        LOG(LOG_DEBUG, "EE_Init >> FLASH_ProgramHalfWord Page 1 \n");        FlashStatus = FLASH_ProgramHalfWord(PAGE1_BASE_ADDRESS, VALID_PAGE);        /* If program operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }        /* Erase Page0 */        LOG(LOG_DEBUG, "EE_Init >> Erase Page 0 \n");        if(0xFFFF != Check_Sector_Erased(PAGE0_ID))        {            LOG(LOG_DEBUG, "Page 0 not blank !! erase it. \n");            FlashStatus = FLASH_EraseSector(PAGE0_ID, VOLTAGE_RANGE);        }        else        {            LOG(LOG_DEBUG, "Page 0 blank, skip to erase it. \n");            FlashStatus = FLASH_COMPLETE;        }        /* If erase operation was failed, a Flash error code is returned */        if (FlashStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);          return FlashStatus;        }      }      break;    default:  /* Any other state -> format eeprom */      LOG(LOG_DEBUG, "EE_Init >> default \n");      /* Erase both Page0 and Page1 and set Page0 as valid page */      LOG(LOG_DEBUG, "EE_Init >> EE_Format \n");      FlashStatus = EE_Format();      /* If erase/program operation was failed, a Flash error code is returned */      if (FlashStatus != FLASH_COMPLETE)      {        LOG(LOG_DEBUG, "EE_Init >> ERROR %d \n", FlashStatus);        return FlashStatus;      }      break;  }  return FLASH_COMPLETE;}/**  * @brief  Returns the last stored variable data, if found, which correspond to  *   the passed virtual address  * @param  VirtAddress: Variable virtual address  * @param  Data: Global variable contains the read variable value  * @retval Success or error status:  *           - 0: if variable was found  *           - 1: if the variable was not found  *           - NO_VALID_PAGE: if no valid page was found.  */uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data){  uint16_t ValidPage = PAGE0;  uint16_t AddressValue = 0x5555, ReadStatus = 1;  uint32_t Address = EEPROM_START_ADDRESS, PageStartAddress = EEPROM_START_ADDRESS;  /* Get active Page for read operation */  ValidPage = EE_FindValidPage(READ_FROM_VALID_PAGE);  /* Check if there is no valid page */  if (ValidPage == NO_VALID_PAGE)  {    return  NO_VALID_PAGE;  }  /* Get the valid Page start Address */  PageStartAddress = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * PAGE_SIZE));  /* Get the valid Page end Address */  Address = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + ValidPage) * PAGE_SIZE));  /* Check each active page address starting from end */  while (Address > (PageStartAddress + 2))  {    /* Get the current location content to be compared with virtual address */    AddressValue = (*(__IO uint16_t*)Address);    /* Compare the read address with the virtual address */    if (AddressValue == VirtAddress)    {      /* Get content of Address-2 which is variable value */      *Data = (*(__IO uint16_t*)(Address - 2));      /* In case variable value is read, reset ReadStatus flag */      ReadStatus = 0;      break;    }    else    {      /* Next address location */      Address = Address - 4;    }  }  /* Return ReadStatus value: (0: variable exist, 1: variable doesn't exist) */  return ReadStatus;}/**  * @brief  Writes/upadtes variable data in EEPROM.  * @param  VirtAddress: Variable virtual address  * @param  Data: 16 bit data to be written  * @retval Success or error status:  *           - FLASH_COMPLETE: on success  *           - PAGE_FULL: if valid page is full  *           - NO_VALID_PAGE: if no valid page was found  *           - Flash error code: on write Flash error  */uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data){  uint16_t Status = 0;  /* Write the variable virtual address and value in the EEPROM */  Status = EE_VerifyPageFullWriteVariable(VirtAddress, Data);  /* In case the EEPROM active page is full */  if (Status == PAGE_FULL)  {    /* Perform Page transfer */    Status = EE_PageTransfer(VirtAddress, Data);  }  /* Return last operation status */  return Status;}/**  * @brief  Erases PAGE and PAGE1 and writes VALID_PAGE header to PAGE  * @param  None  * @retval Status of the last operation (Flash write or erase) done during  *         EEPROM formating  */static FLASH_Status EE_Format(void){  FLASH_Status FlashStatus = FLASH_COMPLETE;  LOG(LOG_DEBUG, "EE_Format \n");  /* Erase Page0 */  if(0xFFFF != Check_Sector_Erased(PAGE0_ID))  {      LOG(LOG_DEBUG, "Page 0 not blank !! erase it. \n");      FlashStatus = FLASH_EraseSector(PAGE0_ID, VOLTAGE_RANGE);  }  else  {      LOG(LOG_DEBUG, "Page 0 blank, skip to erase it. \n");      FlashStatus = FLASH_COMPLETE;  }  /* If erase operation was failed, a Flash error code is returned */  if (FlashStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_Format >> ERROR %d \n", FlashStatus);    return FlashStatus;  }  /* Set Page0 as valid page: Write VALID_PAGE at Page0 base address */  FlashStatus = FLASH_ProgramHalfWord(PAGE0_BASE_ADDRESS, VALID_PAGE);  /* If program operation was failed, a Flash error code is returned */  if (FlashStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_Format >> ERROR %d \n", FlashStatus);    return FlashStatus;  }  /* Erase Page1 */  if(0xFFFF != Check_Sector_Erased(PAGE1_ID))  {      LOG(LOG_DEBUG, "Page 1 not blank !! erase it. \n");      FlashStatus = FLASH_EraseSector(PAGE1_ID, VOLTAGE_RANGE);  }  else  {      LOG(LOG_DEBUG, "Page 1 blank, skip to erase it. \n");      FlashStatus = FLASH_COMPLETE;  }  /* Return Page1 erase operation status */  return FlashStatus;}/**  * @brief  Find valid Page for write or read operation  * @param  Operation: operation to achieve on the valid page.  *   This parameter can be one of the following values:  *     @arg READ_FROM_VALID_PAGE: read operation from valid page  *     @arg WRITE_IN_VALID_PAGE: write operation from valid page  * @retval Valid page number (PAGE or PAGE1) or NO_VALID_PAGE in case  *   of no valid page was found  */static uint16_t EE_FindValidPage(uint8_t Operation){  uint16_t PageStatus0 = 6, PageStatus1 = 6;  /* Get Page0 actual status */  PageStatus0 = (*(__IO uint16_t*)PAGE0_BASE_ADDRESS);  /* Get Page1 actual status */  PageStatus1 = (*(__IO uint16_t*)PAGE1_BASE_ADDRESS);  /* Write or read operation */  switch (Operation)  {    case WRITE_IN_VALID_PAGE:   /* ---- Write operation ---- */      if (PageStatus1 == VALID_PAGE)      {        /* Page0 receiving data */        if (PageStatus0 == RECEIVE_DATA)        {          return PAGE0;         /* Page0 valid */        }        else        {          return PAGE1;         /* Page1 valid */        }      }      else if (PageStatus0 == VALID_PAGE)      {        /* Page1 receiving data */        if (PageStatus1 == RECEIVE_DATA)        {          return PAGE1;         /* Page1 valid */        }        else        {          return PAGE0;         /* Page0 valid */        }      }      else      {        return NO_VALID_PAGE;   /* No valid Page */      }    case READ_FROM_VALID_PAGE:  /* ---- Read operation ---- */      if (PageStatus0 == VALID_PAGE)      {        return PAGE0;           /* Page0 valid */      }      else if (PageStatus1 == VALID_PAGE)      {        return PAGE1;           /* Page1 valid */      }      else      {        return NO_VALID_PAGE ;  /* No valid Page */      }    default:      return PAGE0;             /* Page0 valid */  }}/**  * @brief  Verify if active page is full and Writes variable in EEPROM.  * @param  VirtAddress: 16 bit virtual address of the variable  * @param  Data: 16 bit data to be written as variable value  * @retval Success or error status:  *           - FLASH_COMPLETE: on success  *           - PAGE_FULL: if valid page is full  *           - NO_VALID_PAGE: if no valid page was found  *           - Flash error code: on write Flash error  */static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data){  FLASH_Status FlashStatus = FLASH_COMPLETE;  uint16_t ValidPage = PAGE0;  uint32_t Address = EEPROM_START_ADDRESS, PageEndAddress = EEPROM_START_ADDRESS+PAGE_SIZE;  /* Get valid Page for write operation */  ValidPage = EE_FindValidPage(WRITE_IN_VALID_PAGE);  /* Check if there is no valid page */  if (ValidPage == NO_VALID_PAGE)  {    return  NO_VALID_PAGE;  }  /* Get the valid Page start Address */  Address = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * PAGE_SIZE));  /* Get the valid Page end Address */  PageEndAddress = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + ValidPage) * PAGE_SIZE));  /* Check each active page address starting from begining */  while (Address < PageEndAddress)  {    /* Verify if Address and Address+2 contents are 0xFFFFFFFF */    if ((*(__IO uint32_t*)Address) == 0xFFFFFFFF)    {      /* Set variable data */      FlashStatus = FLASH_ProgramHalfWord(Address, Data);      /* If program operation was failed, a Flash error code is returned */      if (FlashStatus != FLASH_COMPLETE)      {        LOG(LOG_DEBUG, "EE_VerifyPageFullWriteVariable >> ERROR %d \n", FlashStatus);        return FlashStatus;      }      /* Set variable virtual address */      FlashStatus = FLASH_ProgramHalfWord(Address + 2, VirtAddress);      /* Return program operation status */      return FlashStatus;    }    else    {      /* Next address location */      Address = Address + 4;    }  }  /* Return PAGE_FULL in case the valid page is full */  return PAGE_FULL;}/**  * @brief  Transfers last updated variables data from the full Page to  *   an empty one.  * @param  VirtAddress: 16 bit virtual address of the variable  * @param  Data: 16 bit data to be written as variable value  * @retval Success or error status:  *           - FLASH_COMPLETE: on success  *           - PAGE_FULL: if valid page is full  *           - NO_VALID_PAGE: if no valid page was found  *           - Flash error code: on write Flash error  */static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data){  FLASH_Status FlashStatus = FLASH_COMPLETE;  uint32_t NewPageAddress = EEPROM_START_ADDRESS;  uint16_t OldPageId=0;  uint16_t ValidPage = PAGE0, VarIdx = 0;  uint16_t EepromStatus = 0, ReadStatus = 0;  /* Get active Page for read operation */  ValidPage = EE_FindValidPage(READ_FROM_VALID_PAGE);  if (ValidPage == PAGE1)       /* Page1 valid */  {    /* New page address where variable will be moved to */    NewPageAddress = PAGE0_BASE_ADDRESS;    /* Old page ID where variable will be taken from */    OldPageId = PAGE1_ID;  }  else if (ValidPage == PAGE0)  /* Page0 valid */  {    /* New page address  where variable will be moved to */    NewPageAddress = PAGE1_BASE_ADDRESS;    /* Old page ID where variable will be taken from */    OldPageId = PAGE0_ID;  }  else  {    return NO_VALID_PAGE;       /* No valid Page */  }  /* Set the new Page status to RECEIVE_DATA status */  FlashStatus = FLASH_ProgramHalfWord(NewPageAddress, RECEIVE_DATA);  /* If program operation was failed, a Flash error code is returned */  if (FlashStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_PageTransfer >> ERROR %d \n", FlashStatus);    return FlashStatus;  }  /* Write the variable passed as parameter in the new active page */  EepromStatus = EE_VerifyPageFullWriteVariable(VirtAddress, Data);  /* If program operation was failed, a Flash error code is returned */  if (EepromStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_PageTransfer >> ERROR %d \n", EepromStatus);    return EepromStatus;  }  /* Transfer process: transfer variables from old to the new active page */  for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++)  {    if (VirtAddVarTab[VarIdx] != VirtAddress)  /* Check each variable except the one passed as parameter */    {      /* Read the other last variable updates */      ReadStatus = EE_ReadVariable(VirtAddVarTab[VarIdx], &DataVar);      /* In case variable corresponding to the virtual address was found */      if (ReadStatus != 0x1)      {        /* Transfer the variable to the new active page */        EepromStatus = EE_VerifyPageFullWriteVariable(VirtAddVarTab[VarIdx], DataVar);        /* If program operation was failed, a Flash error code is returned */        if (EepromStatus != FLASH_COMPLETE)        {          LOG(LOG_DEBUG, "EE_PageTransfer >> ERROR %d \n", EepromStatus);          return EepromStatus;        }      }    }  }  /* Erase the old Page: Set old Page status to ERASED status */  if(0xFFFF != Check_Sector_Erased(OldPageId))  {      LOG(LOG_DEBUG, "Page %d not blank !! erase it. \n", (OldPageId == PAGE0_ID)? 0:1 );      FlashStatus = FLASH_EraseSector(OldPageId, VOLTAGE_RANGE);  }  else  {      LOG(LOG_DEBUG, "Page %d blank, skip to erase it. \n", (OldPageId == PAGE0_ID)? 0:1 );      FlashStatus = FLASH_COMPLETE;  }  /* If erase operation was failed, a Flash error code is returned */  if (FlashStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_PageTransfer >> ERROR %d \n", FlashStatus);    return FlashStatus;  }  /* Set new Page status to VALID_PAGE status */  FlashStatus = FLASH_ProgramHalfWord(NewPageAddress, VALID_PAGE);  /* If program operation was failed, a Flash error code is returned */  if (FlashStatus != FLASH_COMPLETE)  {    LOG(LOG_DEBUG, "EE_PageTransfer >> ERROR %d \n", FlashStatus);    return FlashStatus;  }  /* Return last operation flash status */  return FlashStatus;}static uint16_t Check_Sector_Erased(uint32_t FLASH_Sector){    /* Check the parameters */    assert_param(IS_FLASH_SECTOR(FLASH_Sector));    uint32_t i = 0 ;    uint32_t SS = flash_sectors[(FLASH_Sector / 8)].base;    uint32_t SE = (SS + flash_sectors[(FLASH_Sector / 8)].size - 1);    while(((*(__IO uint16_t*)(SS + i)) == 0xFFFF) && ((SS + (++i)) < SE));    LOG(LOG_DEBUG, "SS = 0x%08X, SE = 0x%08X, COUNT = 0x%08X  \n", SS, SE,  (i + 1));    if((SS + i) == SE)        return ERASED;    return VALID_PAGE;}struct mtd_dev_s g_internal_flash;FAR struct mtd_dev_s *up_flashinitialize(void){    FAR struct mtd_dev_s *priv = &g_internal_flash;    priv->erase  = internal_flash_erase;    priv->bread  = internal_flash_bread;    priv->bwrite = internal_flash_bwrite;    priv->ioctl  = internal_flash_ioctl;    return priv;}static int internal_flash_eraseall(FAR struct at24c_dev_s *priv){    int ret = OK;    LOG(LOG_DEBUG, "internal_flash_eraseall \n");    if(EE_Format() == FLASH_COMPLETE)        return OK;}static int internal_flash_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks){    LOG(LOG_DEBUG, "internal_flash_erase, startblock = %d, nblocks = %d \n", startblock,nblocks);    return OK;}static ssize_t internal_flash_bread(FAR struct mtd_dev_s *dev, off_t startblock,               size_t nblocks, FAR uint8_t *buf){    LOG(LOG_DEBUG, "internal_flash_bread, startblock = %d, nblocks = %d \n", startblock,nblocks);    return nblocks;}static ssize_t internal_flash_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,                size_t nblocks, FAR const uint8_t *buf){    LOG(LOG_DEBUG, "internal_flash_bwrite, startblock = %d, nblocks = %d \n", startblock,nblocks);    return nblocks;}void internal_flash_test(void){#if 0    uint32_t address = 0, data = 0;    LOG(LOG_DEBUG, "internal_flash_test \n");    LOG(LOG_DEBUG, "Warning !! This test will destroy the data stored on sector 1 and 2 ! \n");    /* prevent other tasks from running while we do this */	sched_lock();    /* Unlock the Flash Program Erase controller */    LOG(LOG_DEBUG, "1)FLASH_Unlock.... \n");    FLASH_Unlock();    /* EEPROM Init */    LOG(LOG_DEBUG, "2)EE_Init.... \n");    EE_Init();#if 0    /* --- Store successively many values of the three variables in the EEPROM ---*/    /* Store 0x1000 values of Variable1 in EEPROM */    LOG(LOG_DEBUG, "3)EE_WriteVariable.... \n");    for (VarValue = 1; VarValue <= 0x1000; VarValue++)    {        LOG(LOG_DEBUG, "VirtAddVarTab[0] = 0x%04X, VarValue = 0x%04X  \n", VirtAddVarTab[0], VarValue);        EE_WriteVariable(VirtAddVarTab[0], VarValue);    }    /* read the last stored variables data*/    LOG(LOG_DEBUG, "4)EE_ReadVariable.... \n");    EE_ReadVariable(VirtAddVarTab[0], &VarDataTab[0]);    LOG(LOG_DEBUG, "VirtAddVarTab[0] = 0x%04X, VarDataTab[0] = 0x%04X  \n", VirtAddVarTab[0], VarDataTab[0]);    /* Store 0x2000 values of Variable2 in EEPROM */    LOG(LOG_DEBUG, "5)EE_WriteVariable.... \n");    for (VarValue = 1; VarValue <= 0x2000; VarValue++)    {        LOG(LOG_DEBUG, "VirtAddVarTab[1] = 0x%04X, VarValue = 0x%04X  \n", VirtAddVarTab[1], VarValue);        EE_WriteVariable(VirtAddVarTab[1], VarValue);    }    /* read the last stored variables data*/    LOG(LOG_DEBUG, "6)EE_ReadVariable.... \n");    EE_ReadVariable(VirtAddVarTab[0], &VarDataTab[0]);    LOG(LOG_DEBUG, "VirtAddVarTab[0] = 0x%04X, VarDataTab[0] = 0x%04X  \n", VirtAddVarTab[0], VarDataTab[0]);    EE_ReadVariable(VirtAddVarTab[1], &VarDataTab[1]);    LOG(LOG_DEBUG, "VirtAddVarTab[1] = 0x%04X, VarDataTab[1] = 0x%04X  \n", VirtAddVarTab[1], VarDataTab[1]);    /* Store 0x3000 values of Variable3 in EEPROM */    LOG(LOG_DEBUG, "7)EE_WriteVariable.... \n");    for (VarValue = 1; VarValue <= 0x3000; VarValue++)    {        LOG(LOG_DEBUG, "VirtAddVarTab[2] = 0x%04X, VarValue = 0x%04X  \n", VirtAddVarTab[2], VarValue);        EE_WriteVariable(VirtAddVarTab[2], VarValue);    }    /* read the last stored variables data*/    LOG(LOG_DEBUG, "8)EE_ReadVariable.... \n");    EE_ReadVariable(VirtAddVarTab[0], &VarDataTab[0]);    LOG(LOG_DEBUG, "VirtAddVarTab[0] = 0x%04X, VarDataTab[0] = 0x%04X  \n", VirtAddVarTab[0], VarDataTab[0]);    EE_ReadVariable(VirtAddVarTab[1], &VarDataTab[1]);    LOG(LOG_DEBUG, "VirtAddVarTab[1] = 0x%04X, VarDataTab[1] = 0x%04X  \n", VirtAddVarTab[1], VarDataTab[1]);    EE_ReadVariable(VirtAddVarTab[2], &VarDataTab[2]);    LOG(LOG_DEBUG, "VirtAddVarTab[2] = 0x%04X, VarDataTab[2] = 0x%04X  \n", VirtAddVarTab[2], VarDataTab[2]);#endif    for(address = 0 ; address < 200 ; address ++ )    {        LOG(LOG_DEBUG, "address = 0x%04X, data = 0x%04X  \n", address, address);        EE_WriteVariable(address, address);    }    for(address = 0 ; address < 200 ; address ++ )    {        EE_ReadVariable(address, &data);        LOG(LOG_DEBUG, "address = 0x%04X, data = 0x%04X  \n", address, data);    }    FLASH_Lock();    /* unlock the scheduler */	sched_unlock();#endif    printf(        "---------------------------------------------------------------------------------------\n");    printf(        "RM0090 Table 5. Flash module organization (STM32F40x and STM32F41x)            \n");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 0  (%s)     \n", Check_Sector_Erased( FLASH_Sector_0)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 1  (%s)     \n", Check_Sector_Erased( FLASH_Sector_1)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 2  (%s)     \n", Check_Sector_Erased( FLASH_Sector_2)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 3  (%s)     \n", Check_Sector_Erased( FLASH_Sector_3)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 4  (%s)     \n", Check_Sector_Erased( FLASH_Sector_4)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 5  (%s)     \n", Check_Sector_Erased( FLASH_Sector_5)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 6  (%s)     \n", Check_Sector_Erased( FLASH_Sector_6)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 7  (%s)     \n", Check_Sector_Erased( FLASH_Sector_7)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 8  (%s)     \n", Check_Sector_Erased( FLASH_Sector_8)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 9  (%s)     \n", Check_Sector_Erased( FLASH_Sector_9)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 10 (%s)     \n", Check_Sector_Erased(FLASH_Sector_10)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Check_Sector_Erased 11 (%s)     \n", Check_Sector_Erased(FLASH_Sector_11)? "ERASED":"DATA");    printf(        "---------------------------------------------------------------------------------------\n");    LOG(LOG_DEBUG, "Warning !! \nThis test will destroy the data stored on sector 1, 2 and 3 ! \n");    /* prevent other tasks from running while we do this */    LOG(LOG_DEBUG, "1)sched_lock.... \n");	sched_lock();    /* Unlock the Flash Program Erase controller */    LOG(LOG_DEBUG, "2)FLASH_Unlock.... \n");    FLASH_Unlock();    /*        *     STM2F4xx (ST3240G) can't clear FLASH error flags (use google serach)       *       *   https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/AllItems.aspx?RootFolder=%2       *   Fpublic%2FSTe2ecommunities%2Fmcu%2FLists%2Fcortex_mx_stm32%2FSTM2F4xx%20(ST3240G)%20can       *   't%20clear%20FLASH%20error%20flags       *       */    #ifndef DONT_FIX_BUG    LOG(LOG_DEBUG, "FLASH->SR (B) = 0x%08X \n", FLASH->SR);    FLASH_ClearFlag(FLASH_FLAG_PGSERR | FLASH_FLAG_PGPERR |                    FLASH_FLAG_PGAERR | FLASH_FLAG_WRPERR |                    FLASH_FLAG_OPERR | FLASH_FLAG_EOP); // clear error status    LOG(LOG_DEBUG, "FLASH->SR (A) = 0x%08X \n", FLASH->SR);    #endif    /* EEPROM Init */    LOG(LOG_DEBUG, "3)EE_Init.... \n");    EE_Init();    LOG(LOG_DEBUG, "4)FLASH_Lock.... \n");    FLASH_Lock();    /* unlock the scheduler */    LOG(LOG_DEBUG, "5)sched_unlock.... \n");	sched_unlock();}static int internal_flash_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg){    FAR struct mtd_dev_s *priv = dev;    int ret = -EINVAL; /* Assume good command with bad parameters */    LOG(LOG_DEBUG, "internal_flash_ioctl, cmd = %d, arg = %d \n", cmd, arg);    fvdbg("cmd: %d \n", cmd);    switch (cmd) {    case MTDIOC_GEOMETRY: {            FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)((uintptr_t)arg);            LOG(LOG_DEBUG, "internal_flash_ioctl > MTDIOC_GEOMETRY \n");            if (geo) {                geo->blocksize    = 32;                geo->erasesize    = 32;                geo->neraseblocks = (48 * 1024 / 16) / 32;                ret               = OK;                fvdbg("blocksize: %d erasesize: %d neraseblocks: %d\n",                      geo->blocksize, geo->erasesize, geo->neraseblocks);            }        }        break;    case MTDIOC_BULKERASE:        LOG(LOG_DEBUG, "internal_flash_ioctl > MTDIOC_BULKERASE \n");        ret = internal_flash_eraseall(priv);        break;    case MTDIOC_XIPBASE:    default:        ret = -ENOTTY; /* Bad command */        break;    }    return ret;}void assert_failed(uint8_t* file, uint32_t line){    LOG(LOG_DEBUG, "assert_failed !! file > %s, line %d \n", file, line);}