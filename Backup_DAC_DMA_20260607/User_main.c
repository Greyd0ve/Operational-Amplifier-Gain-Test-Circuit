#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "AD.h"
#include "MCP4725.h"
#include "SignalGen.h"
#include "Key.h"
#include "App.h"

int main(void)
{
	App_Init();
	
	while (1)
	{
		App_Run();
	}
}
