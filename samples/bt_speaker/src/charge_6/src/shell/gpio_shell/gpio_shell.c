#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shell/shell.h>
#include <mem_manager.h>

#include <gpio.h>
#include <board.h>

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "gpio_shell"
#include <logging/sys_log.h>

#define PIN_IN	            KEY_BROADCAST_PIN
#define PIN_IN_CONFIG     (GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH) // if need GPIO_PUD_PULL_UP or GPIO_PUD_PULL_DOWN ?

struct drv_data {
	struct gpio_callback gpio_cb;
	int mode;
	int index;
};

static struct drv_data cb_data[2];
static int cb_cnt[2];

static void callback_1(struct device *dev,
		       struct gpio_callback *gpio_cb, u32_t pins)
{
	SYS_LOG_INF("triggered: %d\n", ++cb_cnt[0]);

}

static int shell_gpio_config(int argc, char *argv[])
{
    int ret = 0;
	struct device *dev;

    dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!dev) 
	{
		SYS_LOG_ERR("dev not found\n");
		return -1;
	}

    gpio_pin_disable_callback(dev, PIN_IN);

    /* configure PIN_IN callback and trigger condition */
	ret = gpio_pin_configure(dev, PIN_IN, PIN_IN_CONFIG);

    if (ret != 0)
    {
        SYS_LOG_ERR("gpio_pin_configure err\n");
        return -1;
    }

    // gpio_init_callback(&cb_data[0].gpio_cb, callback_1, GPIO_BIT(PIN_IN));
    cb_data[0].gpio_cb.handler = callback_1;
    cb_data[0].gpio_cb.pin_mask = GPIO_BIT(PIN_IN);
    cb_data[0].gpio_cb.pin_group = PIN_IN / 32;

	gpio_add_callback(dev, &cb_data[0].gpio_cb);

    gpio_pin_enable_callback(dev, PIN_IN);

    SYS_LOG_INF("ok\n");

	return 0;
}

static const struct shell_cmd commands[] = {
	{ "gpio_config", shell_gpio_config, "gpio config"},
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("user_gpio", commands);
