#ifndef __KEY_H
#define __KEY_H

typedef enum
{
	KEY_NONE = 0,
	KEY_UP,
	KEY_DOWN,
	KEY_MODE,
	KEY_AUTO
} KeyNum_t;

void Key_Init(void);
KeyNum_t Key_GetNum(void);

#endif
