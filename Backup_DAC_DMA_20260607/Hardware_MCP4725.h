#ifndef __MCP4725_H
#define __MCP4725_H

#include "stm32f10x.h"

void MCP4725_Init(void);
uint8_t MCP4725_WriteFast(uint16_t code);
uint32_t MCP4725_GetErrorCount(void);

#endif
