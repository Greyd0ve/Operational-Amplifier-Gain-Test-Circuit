#include "stm32f10x.h"                  // Device header
#include "MCP4725.h"
#include "AppConfig.h"

#define MCP4725_TIMEOUT                 1000

static uint32_t MCP4725_ErrorCount;

static uint8_t MCP4725_WaitEvent(uint32_t I2C_EVENT)
{
	uint32_t Timeout = MCP4725_TIMEOUT;
	while (I2C_CheckEvent(I2C1, I2C_EVENT) != SUCCESS)
	{
		if (Timeout-- == 0)
		{
			MCP4725_ErrorCount ++;
			return 0;
		}
	}
	return 1;
}

static uint8_t MCP4725_WaitFlag(FlagStatus Status)
{
	uint32_t Timeout = MCP4725_TIMEOUT;
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == Status)
	{
		if (Timeout-- == 0)
		{
			MCP4725_ErrorCount ++;
			return 0;
		}
	}
	return 1;
}

void MCP4725_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	I2C_DeInit(I2C1);
	
	I2C_InitTypeDef I2C_InitStructure;
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 400000;
	I2C_Init(I2C1, &I2C_InitStructure);
	
	I2C_Cmd(I2C1, ENABLE);
	
	MCP4725_WriteFast(DAC_MID_CODE);
}

uint8_t MCP4725_WriteFast(uint16_t code)
{
	uint8_t DataH, DataL;
	
	if (code > DAC_MAX_CODE)
	{
		code = DAC_MAX_CODE;
	}
	
	DataH = code >> 4;
	DataL = (code & 0x0F) << 4;
	
	/* 比赛快速实现方案：TIM2中断内会调用本阻塞式I2C发送函数。
	   MCP4725为I2C DAC，更新率不能设得过高，否则中断占用会明显增加。 */
	if (MCP4725_WaitFlag(SET) == 0)
	{
		I2C_GenerateSTOP(I2C1, ENABLE);
		return 0;
	}
	
	I2C_GenerateSTART(I2C1, ENABLE);
	if (MCP4725_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) == 0)
	{
		I2C_GenerateSTOP(I2C1, ENABLE);
		return 0;
	}
	
	I2C_Send7bitAddress(I2C1, MCP4725_ADDR_8BIT, I2C_Direction_Transmitter);
	if (MCP4725_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == 0)
	{
		I2C_GenerateSTOP(I2C1, ENABLE);
		return 0;
	}
	
	I2C_SendData(I2C1, DataH);
	if (MCP4725_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING) == 0)
	{
		I2C_GenerateSTOP(I2C1, ENABLE);
		return 0;
	}
	
	I2C_SendData(I2C1, DataL);
	if (MCP4725_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) == 0)
	{
		I2C_GenerateSTOP(I2C1, ENABLE);
		return 0;
	}
	
	I2C_GenerateSTOP(I2C1, ENABLE);
	return 1;
}

uint32_t MCP4725_GetErrorCount(void)
{
	return MCP4725_ErrorCount;
}
