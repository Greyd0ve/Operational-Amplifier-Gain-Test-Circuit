#include "stm32f10x.h"
#include "Key.h"
#include "AppConfig.h"

#define KEY_DEBOUNCE_TICKS      2
#define KEY_LONG_TICKS          (KEY_LONG_PRESS_MS / KEY_SCAN_PERIOD_MS)

typedef struct
{
    uint8_t sample_level;
    uint8_t stable_level;
    uint8_t debounce_count;
    uint16_t press_ticks;
    uint8_t long_reported;
    KeyEvent_t short_event;
    KeyEvent_t long_event;
} KeyState_t;

static KeyState_t Key_State[3];
static volatile KeyEvent_t Key_Event = KEY_EVENT_NONE;

static uint8_t Key_IsPressed(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t active_level)
{
    uint8_t level;

    level = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
    return (level == active_level) ? 1 : 0;
}

static void Key_ConfigGPIO(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t active_level)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    if (active_level)
    {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    }
    else
    {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    }

    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

static void Key_PushEvent(KeyEvent_t event)
{
    if (Key_Event == KEY_EVENT_NONE)
    {
        Key_Event = event;
    }
}

static void Key_ScanOne(KeyState_t *state, uint8_t pressed)
{
    if (pressed != state->sample_level)
    {
        state->sample_level = pressed;
        state->debounce_count = 0;
    }
    else
    {
        if (state->debounce_count < KEY_DEBOUNCE_TICKS)
        {
            state->debounce_count++;
        }
        else if (state->stable_level != state->sample_level)
        {
            state->stable_level = state->sample_level;

            if (state->stable_level)
            {
                state->press_ticks = 0;
                state->long_reported = 0;
            }
            else
            {
                if (!state->long_reported && (state->press_ticks < KEY_LONG_TICKS))
                {
                    Key_PushEvent(state->short_event);
                }
            }
        }
    }

    if (state->stable_level)
    {
        if (state->press_ticks < 60000)
        {
            state->press_ticks++;
        }

        if (!state->long_reported && (state->press_ticks >= KEY_LONG_TICKS))
        {
            state->long_reported = 1;
            Key_PushEvent(state->long_event);
        }
    }
}

void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(KEY1_RCC | KEY2_RCC | KEY3_RCC, ENABLE);

    Key_ConfigGPIO(KEY1_GPIO, KEY1_PIN, KEY1_ACTIVE_LEVEL);
    Key_ConfigGPIO(KEY2_GPIO, KEY2_PIN, KEY2_ACTIVE_LEVEL);
    Key_ConfigGPIO(KEY3_GPIO, KEY3_PIN, KEY3_ACTIVE_LEVEL);

    Key_State[0].sample_level = 0;
    Key_State[0].stable_level = 0;
    Key_State[0].debounce_count = 0;
    Key_State[0].press_ticks = 0;
    Key_State[0].long_reported = 0;
    Key_State[0].short_event = KEY1_SHORT;
    Key_State[0].long_event = KEY1_LONG;

    Key_State[1].sample_level = 0;
    Key_State[1].stable_level = 0;
    Key_State[1].debounce_count = 0;
    Key_State[1].press_ticks = 0;
    Key_State[1].long_reported = 0;
    Key_State[1].short_event = KEY2_SHORT;
    Key_State[1].long_event = KEY2_LONG;

    Key_State[2].sample_level = 0;
    Key_State[2].stable_level = 0;
    Key_State[2].debounce_count = 0;
    Key_State[2].press_ticks = 0;
    Key_State[2].long_reported = 0;
    Key_State[2].short_event = KEY3_SHORT;
    Key_State[2].long_event = KEY3_LONG;

    Key_Event = KEY_EVENT_NONE;
}

void Key_Scan10ms(void)
{
    Key_ScanOne(&Key_State[0], Key_IsPressed(KEY1_GPIO, KEY1_PIN, KEY1_ACTIVE_LEVEL));
    Key_ScanOne(&Key_State[1], Key_IsPressed(KEY2_GPIO, KEY2_PIN, KEY2_ACTIVE_LEVEL));
    Key_ScanOne(&Key_State[2], Key_IsPressed(KEY3_GPIO, KEY3_PIN, KEY3_ACTIVE_LEVEL));
}

KeyEvent_t Key_GetEvent(void)
{
    KeyEvent_t event;

    event = Key_Event;
    Key_Event = KEY_EVENT_NONE;
    return event;
}