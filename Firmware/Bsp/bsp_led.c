#include "bsp_led.h"

#define LED_GPIO_PORT   GPIOC
#define LED_GPIO_PIN    GPIO_Pin_13
#define LED_GPIO_CLK    RCC_APB2Periph_GPIOC

void led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(LED_GPIO_CLK, ENABLE);

    gpio.GPIO_Pin   = LED_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LED_GPIO_PORT, &gpio);

    led_off();
}

/* PC13 低电平点亮，故 ON = 拉低 */
void led_on(void)
{
    GPIO_ResetBits(LED_GPIO_PORT, LED_GPIO_PIN);
}

void led_off(void)
{
    GPIO_SetBits(LED_GPIO_PORT, LED_GPIO_PIN);
}

void led_toggle(void)
{
    LED_GPIO_PORT->ODR ^= LED_GPIO_PIN;
}
