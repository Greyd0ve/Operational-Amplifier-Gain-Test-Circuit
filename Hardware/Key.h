#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY1_SHORT,
    KEY1_LONG,
    KEY2_SHORT,
    KEY2_LONG,
    KEY3_SHORT,
    KEY3_LONG
} KeyEvent_t;

void Key_Init(void);
void Key_Scan10ms(void);
KeyEvent_t Key_GetEvent(void);

#endif
