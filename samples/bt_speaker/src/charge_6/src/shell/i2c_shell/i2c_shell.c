#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shell/shell.h>
#include <mem_manager.h>

#include <i2c.h>

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "i2c_shell"
#include <logging/sys_log.h>

const char *i2c_dev_name_array[] = {
#ifdef CONFIG_I2C_0
	CONFIG_I2C_0_NAME,
#endif

#ifdef CONFIG_I2C_GPIO_0
	CONFIG_I2C_GPIO_0_NAME,
#endif

#ifdef CONFIG_I2C_GPIO_1
	CONFIG_I2C_GPIO_1_NAME,
#endif
};

static int shell_i2c_config(int argc, char *argv[])
{
	struct device *i2c_dev;
	char *i2c_dev_name = NULL;
	int i2c_dev_name_index;
	int i2c_speed_rate = 0;
	union dev_config i2c_dev_config = {.raw = 0};
	int argv_index = 0;

	if (argc != 3)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

	argv_index = 1;

	i2c_dev_name_index = atoi(argv[argv_index++]);

	if (i2c_dev_name_index < ARRAY_SIZE(i2c_dev_name_array))
	{
		i2c_dev_name = (char *)i2c_dev_name_array[i2c_dev_name_index];
	}

	i2c_speed_rate = atoi(argv[argv_index++]);

	if (i2c_speed_rate != 0 && 
		i2c_speed_rate != I2C_SPEED_STANDARD && 
		i2c_speed_rate != I2C_SPEED_FAST)
	{
		SYS_LOG_ERR("i2c_speed_rate param err\n");
		return -1;
	}

	i2c_dev = device_get_binding(i2c_dev_name);
	if (!i2c_dev) 
	{
		SYS_LOG_ERR("no i2c dev\n");
		return -1;
	}

	SYS_LOG_INF("i2c_dev_name:%s\n", i2c_dev_name);
	SYS_LOG_INF("i2c_speed_rate:%d\n", i2c_speed_rate);

	i2c_dev_config.bits.speed = i2c_speed_rate;
	i2c_configure(i2c_dev, i2c_dev_config.raw);

	return 0;
}

// i2c_dev_name + str_buf + i2c_dev_addr
static int shell_i2c_write(int argc, char *argv[])
{
	struct device *i2c_dev;
	char *i2c_dev_name = NULL;
	int i2c_dev_name_index;
	uint8_t *str_buf = NULL;
	int str_buf_size;
	int i2c_dev_addr;

	int i2c_result = 0;
	int argv_index = 0;

	if (argc != 4)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

#if 1 
	//dump all cmd string.
	for (int i=0; i<argc; i++)
	{
		SYS_LOG_INF("[%d] = %s\n", i, argv[i]);
	}
#endif

	argv_index = 1;

	i2c_dev_name_index = atoi(argv[argv_index++]);

	if (i2c_dev_name_index < ARRAY_SIZE(i2c_dev_name_array))
	{
		i2c_dev_name = (char *)i2c_dev_name_array[i2c_dev_name_index];
	}

	str_buf = argv[argv_index++];
	str_buf_size = strlen(str_buf);

	i2c_dev_addr = atoi(argv[argv_index++]);

	SYS_LOG_INF("i2c_dev_name:%s\n", i2c_dev_name);
	SYS_LOG_INF("str_buf_size:%d, str_buf:%s\n", str_buf_size, str_buf);
	SYS_LOG_INF("i2c_dev_addr:0x%X\n", i2c_dev_addr);

    i2c_dev = device_get_binding(i2c_dev_name);
	if (!i2c_dev) 
	{
		SYS_LOG_ERR("no i2c dev\n");
		return -1;
	}

	i2c_result = i2c_write(i2c_dev, str_buf, str_buf_size, i2c_dev_addr);

	if (i2c_result != 0)
	{
		SYS_LOG_ERR("i2c operate error\n");
		return -1;
	}

	SYS_LOG_INF("ok\n");

	return 0;
}

// i2c_dev_name + read_bytes_num + i2c_dev_addr
static int shell_i2c_read(int argc, char *argv[])
{
	struct device *i2c_dev;
	char *i2c_dev_name = NULL;
	int i2c_dev_name_index;
	int i2c_read_bytes_num;
	int i2c_dev_addr;

	uint8_t *p_i2c_read_buf;

	int i2c_result = 0;
	int argv_index = 0;

	if (argc != 4)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

#if 1 
	//dump all cmd string.
	for (int i=0; i<argc; i++)
	{
		SYS_LOG_INF("[%d] = %s\n", i, argv[i]);
	}
#endif

	argv_index = 1;

	i2c_dev_name_index = atoi(argv[argv_index++]);

	if (i2c_dev_name_index < ARRAY_SIZE(i2c_dev_name_array))
	{
		i2c_dev_name = (char *)i2c_dev_name_array[i2c_dev_name_index];
	}

	i2c_read_bytes_num = atoi(argv[argv_index++]);

	i2c_dev_addr = atoi(argv[argv_index++]);

	SYS_LOG_INF("i2c_dev_name:%s\n", i2c_dev_name);
	SYS_LOG_INF("i2c_read_bytes_num:%d\n", i2c_read_bytes_num);
	SYS_LOG_INF("i2c_dev_addr:0x%X\n", i2c_dev_addr);

    i2c_dev = device_get_binding(i2c_dev_name);
	if (!i2c_dev) 
	{
		SYS_LOG_ERR("no i2c dev\n");
		return -1;
	}

	p_i2c_read_buf = malloc(i2c_read_bytes_num);
	if (p_i2c_read_buf == NULL)
	{
		SYS_LOG_ERR("read buf malloc fail\n");
		return -1;
	}

	i2c_result = i2c_read(i2c_dev, p_i2c_read_buf, i2c_read_bytes_num, i2c_dev_addr);

	if (i2c_result != 0)
	{
		SYS_LOG_ERR("i2c operate error\n");
		free(p_i2c_read_buf);
		return -1;
	}

	print_buffer(p_i2c_read_buf, 1, i2c_read_bytes_num, 16, 0);

	free(p_i2c_read_buf);

	SYS_LOG_INF("ok\n");

	return 0;
}

// i2c_dev_name + str_buf + i2c_dev_addr + i2c_start_addr
static int shell_i2c_burst_write(int argc, char *argv[])
{
	struct device *i2c_dev;
	char *i2c_dev_name = NULL;
	int i2c_dev_name_index;
	uint8_t *str_buf = NULL;
	int str_buf_size;
	int i2c_dev_addr;
	int i2c_start_addr;

	int i2c_result = 0;
	int argv_index = 0;

	if (argc != 5)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

#if 1 
	//dump all cmd string.
	for (int i=0; i<argc; i++)
	{
		SYS_LOG_INF("[%d] = %s\n", i, argv[i]);
	}
#endif

	argv_index = 1;

	i2c_dev_name_index = atoi(argv[argv_index++]);

	if (i2c_dev_name_index < ARRAY_SIZE(i2c_dev_name_array))
	{
		i2c_dev_name = (char *)i2c_dev_name_array[i2c_dev_name_index];
	}

	str_buf = argv[argv_index++];
	str_buf_size = strlen(str_buf);

	i2c_dev_addr = atoi(argv[argv_index++]);

	i2c_start_addr = atoi(argv[argv_index++]);

	SYS_LOG_INF("i2c_dev_name:%s\n", i2c_dev_name);
	SYS_LOG_INF("str_buf_size:%d, str_buf:%s\n", str_buf_size, str_buf);
	SYS_LOG_INF("i2c_dev_addr:0x%X\n", i2c_dev_addr);
	SYS_LOG_INF("i2c_start_addr:0x%X\n", i2c_start_addr);

    i2c_dev = device_get_binding(i2c_dev_name);
	if (!i2c_dev) 
	{
		SYS_LOG_ERR("no i2c dev\n");
		return -1;
	}

	i2c_result = i2c_burst_write(i2c_dev, i2c_dev_addr, i2c_start_addr, str_buf, str_buf_size);

	if (i2c_result != 0)
	{
		SYS_LOG_ERR("i2c operate error\n");
		return -1;
	}

	SYS_LOG_INF("ok\n");

	return 0;
}

// i2c_dev_name + read_bytes_num + i2c_dev_addr + i2c_start_addr
static int shell_i2c_burst_read(int argc, char *argv[])
{
	struct device *i2c_dev;
	char *i2c_dev_name = NULL;
	int i2c_dev_name_index;
	int i2c_read_bytes_num;
	int i2c_dev_addr;
	int i2c_start_addr;

	int i2c_result;

	uint8_t *p_i2c_read_buf;
	int argv_index = 0;

	if (argc != 5)
	{
		SYS_LOG_ERR("argc num err\n");
		return -1;
	}

#if 1 
	//dump all cmd string.
	for (int i=0; i<argc; i++)
	{
		SYS_LOG_INF("[%d] = %s\n", i, argv[i]);
	}
#endif

	argv_index = 1;

	i2c_dev_name_index = atoi(argv[argv_index++]);

	if (i2c_dev_name_index < ARRAY_SIZE(i2c_dev_name_array))
	{
		i2c_dev_name = (char *)i2c_dev_name_array[i2c_dev_name_index];
	}

	i2c_read_bytes_num = atoi(argv[argv_index++]);

	i2c_dev_addr = atoi(argv[argv_index++]);

	i2c_start_addr = atoi(argv[argv_index++]);

	SYS_LOG_INF("i2c_dev_name:%s\n", i2c_dev_name);
	SYS_LOG_INF("i2c_read_bytes_num:%d\n", i2c_read_bytes_num);
	SYS_LOG_INF("i2c_dev_addr:0x%X\n", i2c_dev_addr);
	SYS_LOG_INF("i2c_start_addr:0x%X\n", i2c_start_addr);

    i2c_dev = device_get_binding(i2c_dev_name);
	if (!i2c_dev) 
	{
		SYS_LOG_ERR("no i2c dev\n");
		return -1;
	}

	p_i2c_read_buf = malloc(i2c_read_bytes_num);
	if (p_i2c_read_buf == NULL)
	{
		SYS_LOG_ERR("read buf malloc fail\n");
		return -1;
	}

	i2c_result = i2c_burst_read(i2c_dev, i2c_dev_addr, i2c_start_addr, p_i2c_read_buf, i2c_read_bytes_num);

	if (i2c_result != 0)
	{
		SYS_LOG_ERR("i2c operate error\n");
		free(p_i2c_read_buf);
		return -1;
	}

	print_buffer(p_i2c_read_buf, 1, i2c_read_bytes_num, 16, 0);
	
	free(p_i2c_read_buf);

	SYS_LOG_INF("ok\n");

	return 0;
}

static const struct shell_cmd commands[] = {
	{ "i2c_config", shell_i2c_config, "i2c config"},
    { "i2c_write", shell_i2c_write, "i2c write"},
    { "i2c_read", shell_i2c_read, "i2c read"},
    { "i2c_burst_write", shell_i2c_burst_write, "i2c burst write"},
    { "i2c_burst_read", shell_i2c_burst_read, "i2c burst read"},
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("user_i2c", commands);
