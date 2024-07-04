#ifndef __AW9523B_HEAD_H__
#define __AW9523B_HEAD_H__

#define AW8523B_PORT_NR						2
#define AW8523B_PORT_0						0
#define AW8523B_PORT_1						1

#define AW8523B_PIN_NR						8
#define AW8523B_PIN_0						0
#define AW8523B_PIN_1						1
#define AW8523B_PIN_2						2
#define AW8523B_PIN_3						3
#define AW8523B_PIN_4						4
#define AW8523B_PIN_5						5
#define AW8523B_PIN_6						6
#define AW8523B_PIN_7						7

#define AW8523B_MODE_OUTPUT					0
#define AW8523B_MODE_INPUT					1

#define AW8523B_LEVEL_LOW					0
#define AW8523B_LEVEL_HIGH					1

#define AW8523B_LED_LIGHT_ON				AW8523B_LEVEL_LOW
#define AW8523B_LED_LIGHT_OFF				AW8523B_LEVEL_HIGH

#define AW8523B_INT_DISABLE					0
#define AW8523B_INT_ENABLE					1

#define AW8523B_I2C_TRANSMIT_DISABLE		0
#define AW8523B_I2C_TRANSMIT_ENABLE			1



#define LED_GET_PORT(_led)					((_led) >> 3)
#define LED_GET_PIN(_led)					((_led) & 0x07)
#define LED_PORT_PIN(_port, _pin)			((_port << 3) | (_pin))

#define FLIP7_LED1							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_0)
#define FLIP7_LED2							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_1)
#define FLIP7_LED3							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_2)
#define FLIP7_LED4							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_3)
#define FLIP7_LED5							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_4)
#define FLIP7_LED6							LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_5)
#define FLIP7_LED_BT						LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_6)
#define FLIP7_LED_RING					    LED_PORT_PIN(AW8523B_PORT_0, AW8523B_PIN_7)

#define FLIP7_GPIO_OUT_DSP_1V2_CTRL			LED_PORT_PIN(AW8523B_PORT_1, AW8523B_PIN_0)
#define FLIP7_GPIO_OUT_CHARGE_EN			LED_PORT_PIN(AW8523B_PORT_1, AW8523B_PIN_1)
#define FLIP7_GPIO_OUT_CONTROL_19V			LED_PORT_PIN(AW8523B_PORT_1, AW8523B_PIN_2)
#define FLIP7_GPIO_OUT_AMP_5V2_EN			LED_PORT_PIN(AW8523B_PORT_1, AW8523B_PIN_3)

#define FLIP7_GPIO_IN_CHARGE_STATUS			LED_PORT_PIN(AW8523B_PORT_1, AW8523B_PIN_7)




extern int io_expend_aw9523b_gpio_output(int i2c_transmit_en, int port, int pin, int level);
extern int io_expend_aw9523b_gpio_input(int i2c_transmit_en, int port, int pin);
extern int io_expend_aw9523b_registers_init(void);
extern void aw9523b_set_led_status(uint8_t num,uint8_t status);
extern uint8_t aw9523b_get_led_status(uint8_t num);
extern void io_expend_aw9523b_ctl_20v5_set(uint8_t onoff);
extern void aw9523b_set_battery_low_red_led_status(void);
#endif
