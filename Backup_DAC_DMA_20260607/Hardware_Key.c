#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Key.h"
#include "AppConfig.h"

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = KEY_UP_PIN | KEY_DOWN_PIN | KEY_AUTO_PIN | KEY_MODE_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * 函    数：获取按键键码
  * 返 回 值：KEY_NONE / KEY_UP / KEY_DOWN / KEY_MODE / KEY_AUTO
  * 说    明：阻塞式消抖写法简单可靠，但按住按键会短时间影响主循环实时性。
  */
KeyNum_t Key_GetNum(void)
{
	KeyNum_t KeyNum = KEY_NONE;
	
	if (GPIO_ReadInputDataBit(KEY_UP_PORT, KEY_UP_PIN) == 0)
	{
		Delay_ms(20);
		while (GPIO_ReadInputDataBit(KEY_UP_PORT, KEY_UP_PIN) == 0);
		Delay_ms(20);
		KeyNum = KEY_UP;
	}
	
	if (GPIO_ReadInputDataBit(KEY_DOWN_PORT, KEY_DOWN_PIN) == 0)
	{
		Delay_ms(20);
		while (GPIO_ReadInputDataBit(KEY_DOWN_PORT, KEY_DOWN_PIN) == 0);
		Delay_ms(20);
		KeyNum = KEY_DOWN;
	}
	
	if (GPIO_ReadInputDataBit(KEY_AUTO_PORT, KEY_AUTO_PIN) == 0)
	{
		Delay_ms(20);
		while (GPIO_ReadInputDataBit(KEY_AUTO_PORT, KEY_AUTO_PIN) == 0);
		Delay_ms(20);
		KeyNum = KEY_AUTO;
	}
	
	if (GPIO_ReadInputDataBit(KEY_MODE_PORT, KEY_MODE_PIN) == 0)
	{
		Delay_ms(20);
		while (GPIO_ReadInputDataBit(KEY_MODE_PORT, KEY_MODE_PIN) == 0);
		Delay_ms(20);
		KeyNum = KEY_MODE;
	}
	
	return KeyNum;
}
