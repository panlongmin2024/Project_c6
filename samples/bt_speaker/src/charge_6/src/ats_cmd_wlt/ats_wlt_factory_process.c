
#include "ats_wlt_factory_process.h"

#ifdef CONFIG_WLT_ATS_ENABLE

#define	CONFIG_WLT_ATS_NEED_COMM	1

ats_wlt_uart ats_wlt_uart_context;
static struct _wlt_driver_ctx_t *p_ats_info;
static struct _ats_wlt_var *p_ats_var;
static uint8_t *ats_wlt_cmd_resp_buf;
static int ats_wlt_cmd_resp_buf_size = ATS_WLT_UART_TX_LEN_MAX;
static bool isWltAtsMode = false;

struct thread_timer user_test_timer;
ats_wlt_uart ats_wlt_uart_enter;

#if CONFIG_WLT_ATS_NEED_COMM
static struct _wlt_driver_ctx_t *p_ats_wlt_info;

#endif

extern int trace_print_disable_set(unsigned int print_disable);
extern void console_input_deinit(struct device *dev);
extern struct device *uart_console_dev;
extern int trace_dma_print_set(unsigned int dma_enable);
void ats_wlt_write_data(unsigned char *buf, int len);
int ats_wlt_deinit(void);
uint8_t ReadODM(void);

int ats_wlt_check_adfu(void)
{
	bool key_bt_status;
	bool key_vol_up;
	bool dc_power_in_status;

	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	u32_t val;
	gpio_pin_configure(gpio_dev, 21, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
	gpio_pin_configure(gpio_dev, 39, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
	k_sleep(10);
	gpio_pin_read(gpio_dev, 21, &val);
	key_vol_up = (bool)val;
	gpio_pin_read(gpio_dev, 39, &val);
	key_bt_status = (bool)val;
	dc_power_in_status = dc_power_in_status_read();
	SYS_LOG_INF("key_bt down:%d, key_vol_up down:%d, dc_power_in insert:%d\n",
			key_bt_status, key_vol_up, dc_power_in_status);	
	if(key_bt_status == 1 && key_vol_up == 1 && dc_power_in_status == 1){
		sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	}	
	return 0;
}

static void ats_wlt_enter_write_data(unsigned char *buf, int len)
{
	ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
	stream_write(ats_uart->uio, buf, len);	
}
static void ats_wlt_enter_success(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_enter_write_data(ATS_SEND_ENTER_WLT_ATS_ACK,sizeof(ATS_SEND_ENTER_WLT_ATS_ACK)-1);
	isWltAtsMode = true;
}
#if CONFIG_WLT_ATS_NEED_COMM
static int ats_wlt_command_handler(struct device *dev, u8_t *buf, int size)
{
	int index = 0;

	if (!memcmp(&buf[index], ATS_CMD_ENTER_WLT_ATS, sizeof(ATS_CMD_ENTER_WLT_ATS)-1)){
		ats_wlt_enter_success(dev, buf, size);
	}
	else{

	}
	return 0;
}
static int ats_wlt_enter_comm_data_handler(struct device *dev)
{
	ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
	int rx_size=0;
	 
	int s1;
	do {
		s1 = stream_read(ats_uart->uio, ats_uart->rx_buffer+rx_size, 1);
		rx_size += s1;
	} while (s1 > 0);

	memcpy(p_ats_wlt_info->data_buf, ats_uart->rx_buffer, rx_size);

	if (rx_size == 0){
		return 0;
	}
	if(dev == NULL){
		ats_wlt_enter_write_data("dev == NULL",sizeof("dev == NULL")-1);
		return 0;
	}
	stream_write(ats_uart->uio, p_ats_wlt_info->data_buf, rx_size);
	ats_wlt_command_handler(dev, p_ats_wlt_info->data_buf, rx_size);
	return 0;
}

static int ats_wlt_enter_uart_init(struct device *dev)
{
    ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
    struct uart_stream_param uparam;

    console_input_deinit(dev);
	
    ats_uart->uio_opened = 0;

    uparam.uart_dev_name = "UART_0";
    uparam.uart_baud_rate = CONFIG_UART_ACTS_PORT_0_BAUD_RATE;
    ats_uart->uio = uart_stream_create( &uparam.uart_dev_name);
    if(!ats_uart->uio){
       SYS_LOG_ERR("stream create fail");
       return ats_uart->uio_opened;
    }
    stream_open(ats_uart->uio, MODE_IN_OUT);
    ats_uart->uio_opened = 1;

    return ats_uart->uio_opened;
}
static int ats_wlt_enter_uart_deinit(struct device *dev)
{
	ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
	stream_close(ats_uart->uio);
}
/* wait communicate from PC! */
static int ats_wlt_wait_comm(struct device *dev)
{
	int ret = -1;
	int times = 15;
	while(times--){
		ats_wlt_enter_write_data(ATS_SEND_ENTER_WLT_ATS,sizeof(ATS_SEND_ENTER_WLT_ATS)-1);
	
		ats_wlt_enter_comm_data_handler(dev);
		if(isWltAtsMode){
			break;
		}
		k_sleep(20);
	}
	return ret;
}
#endif
int ats_wlt_enter(void)
{
	int ret = -1;
	SYS_LOG_INF("check wlt ats ! odm=%d\n",ReadODM());
	
	if(ReadODM() == 0){
		k_sleep(20);
		if(ReadODM() == 0){
			/* is wlt factory test ! */
			SYS_LOG_INF("real entering wlt ats !\n");

#if CONFIG_WLT_ATS_NEED_COMM
			p_ats_wlt_info = malloc(sizeof(struct _wlt_driver_ctx_t));
			if(p_ats_wlt_info == NULL){
				SYS_LOG_ERR("uart device not found\n");
				return -1;				
			}
			/* init uart! */
			p_ats_wlt_info->ats_uart_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
			if (p_ats_wlt_info->ats_uart_dev == NULL)
			{
				SYS_LOG_ERR("uart device not found\n");
				return -1;	
			}
			ats_wlt_enter_uart_init(p_ats_wlt_info->ats_uart_dev);
			ret = ats_wlt_wait_comm(p_ats_wlt_info->ats_uart_dev);
			free(p_ats_wlt_info);
			console_input_deinit(p_ats_wlt_info->ats_uart_dev);
			ats_wlt_enter_uart_deinit(p_ats_wlt_info->ats_uart_dev);
#else	
			ats_wlt_enter_success(0,0,0);
#endif
		}
	}
	return ret;
}

bool get_enter_wlt_ats_state(void)
{
	SYS_LOG_INF("check wlt ats ! isWltAtsMode %d\n",isWltAtsMode);
	return isWltAtsMode;
}

struct k_msgq *get_ats_wlt_factory_thread_msgq(void)
{
	return &p_ats_info->msgq;
}
static inline os_mutex *ats_wlt_get_mutex(void)
{
    return &p_ats_var->ats_mutex;
}

// 
/*void hex_to_string_4(u32_t num, u8_t *buf) {
	buf[0] = '0' + num%10000/1000;
	buf[1] = '0' + num%1000/100;
	buf[2] = '0' + num%100/10;
	buf[3] = '0' + num%10;
}
void hex_to_string_2(u32_t num, u8_t *buf) {
	buf[0] = '0' + num%100/10;
	buf[1] = '0' + num%10;
}
void string_to_hex_u8(u8_t *buf,u8_t *num) {
	*num = (buf[0]-'0')*10 + (buf[1]-'0');
}*/

static int ats_wlt_resp_init(void)
{

	if (ats_wlt_cmd_resp_buf == NULL)
	{
		ats_wlt_cmd_resp_buf = malloc(ats_wlt_cmd_resp_buf_size);
		if (ats_wlt_cmd_resp_buf == NULL)
		{
	
			return 0;
		}
		memset(ats_wlt_cmd_resp_buf, 0, ats_wlt_cmd_resp_buf_size);
	}

	return 0;
}

static int ats_wlt_cmd_response_ok_or_fail(struct device *dev, u8_t is_ok)
{
	int index = 0;

	if (!ats_wlt_cmd_resp_buf)
	{
		return -1;
	}
	if (is_ok)
	{
      ats_wlt_cmd_resp_buf_size = sizeof(ATS_AT_CMD_RESP_OK)+sizeof(ATS_AT_CMD_WLT_TAIL)-2;
	}
	else
	{
     ats_wlt_cmd_resp_buf_size = sizeof(ATS_AT_CMD_RESP_FAIL)+sizeof(ATS_AT_CMD_WLT_TAIL)-2;
	}
		
	memset(ats_wlt_cmd_resp_buf, 0, ats_wlt_cmd_resp_buf_size);

	if (is_ok)
	{
		memcpy(&ats_wlt_cmd_resp_buf[index], ATS_AT_CMD_RESP_OK, sizeof(ATS_AT_CMD_RESP_OK)-1);
		index += sizeof(ATS_AT_CMD_RESP_OK)-1;
	}
	else
	{
		memcpy(&ats_wlt_cmd_resp_buf[index], ATS_AT_CMD_RESP_FAIL, sizeof(ATS_AT_CMD_RESP_FAIL)-1);
		index += sizeof(ATS_AT_CMD_RESP_FAIL)-1;
	}
	memcpy(&ats_wlt_cmd_resp_buf[index], ATS_AT_CMD_WLT_TAIL, sizeof(ATS_AT_CMD_WLT_TAIL)-1);

	ats_wlt_write_data(ats_wlt_cmd_resp_buf, ats_wlt_cmd_resp_buf_size);

	return 0;
}

int ats_wlt_response_at_data(struct device *dev, u8_t *cmd, int cmd_len, u8_t *ext_data, int ext_data_len)
{
	int index = 0;

	if (!ats_wlt_cmd_resp_buf)
	{
		return -1;
	}
	ats_wlt_cmd_resp_buf_size = cmd_len + ext_data_len + sizeof(ATS_AT_CMD_WLT_TAIL)-1;

	memset(ats_wlt_cmd_resp_buf, 0, ats_wlt_cmd_resp_buf_size);

	if (cmd != NULL || cmd_len != 0)
	{
		memcpy(&ats_wlt_cmd_resp_buf[index], cmd, cmd_len);
		index += cmd_len;
	}
	
	if (ext_data != NULL || ext_data_len != 0)
	{
		memcpy(&ats_wlt_cmd_resp_buf[index], ext_data, ext_data_len);
		index += ext_data_len;
	}
	
    memcpy(&ats_wlt_cmd_resp_buf[index], ATS_AT_CMD_WLT_TAIL, sizeof(ATS_AT_CMD_WLT_TAIL)-1);
	ats_wlt_write_data(ats_wlt_cmd_resp_buf, ats_wlt_cmd_resp_buf_size);

	return 0;
}

static int ats_wlt_shell_set_btedr_mac(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_set_btble_mac(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_get_btedr_mac(struct device *dev, u8_t *buf, int len)
{
	int result;
	char buffer[12+1] = {0};

	result = ats_bt_dev_mac_addr_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{
		ats_wlt_response_at_data(
			dev, ATS_RESP_GET_BTEDR_MAC, sizeof(ATS_RESP_GET_BTEDR_MAC)-1, 
			NULL, 0);
	}
	else 
	{
		ats_wlt_response_at_data(
			dev, ATS_RESP_GET_BTEDR_MAC, sizeof(ATS_RESP_GET_BTEDR_MAC)-1, 
			buffer, sizeof(buffer)-1);
	}

	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_get_btble_mac(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_NG);
	return 0;
}
static int ats_wlt_shell_set_btedr_name(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_NG);
	return 0;
}static int ats_wlt_shell_set_btble_name(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_NG);
	return 0;
}
static int ats_wlt_shell_get_btedr_name(struct device *dev, u8_t *buf, int len)
{
	int result;
	char buffer[32+1] = {0};

	result = ats_bt_dev_name_read(buffer, sizeof(buffer)-1);
	if (result < 0)
	{	
		ats_wlt_response_at_data(
			dev, ATS_RESP_GET_BTEDR_NAME, sizeof(ATS_RESP_GET_BTEDR_NAME)-1, 
			NULL, 0);
	}
	else 
	{
		ats_wlt_response_at_data(
			dev, ATS_RESP_GET_BTEDR_NAME, sizeof(ATS_RESP_GET_BTEDR_NAME)-1, 
			buffer, strlen(buffer));
	}

	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_get_btble_name(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_NG);
	return 0;
}
static int ats_wlt_shell_get_firmware_version(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_gpio_test(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_harman_key_write(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_enter_signal_test_mode(struct device *dev, u8_t *buf, int len)
{
	return 0;
}
static int ats_wlt_shell_enter_nonsignal_test_mode(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	return 0;
}
static int ats_wlt_shell_enter_adfu(struct device *dev, u8_t *buf, int len)
{
	ats_wlt_cmd_response_ok_or_fail(dev,RET_OK);
	sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	return 0;
}
static int ats_wlt_shell_system_reset(struct device *dev, u8_t *buf, int len)
{
	u8_t buffre[1+1] = {6,0};
	/* save reboot flag! */
	property_set(CFG_USER_ATS_REBOOT_SYSTEM,buffre,1);
	property_flush(CFG_USER_ATS_REBOOT_SYSTEM);
	sys_pm_reboot(0);
	return 0;
}

int ats_wlt_command_shell_handler(struct device *dev, u8_t *buf, int size)
{
	int index = 0;
	//int target_index;
	static u8_t init_flag;

	if (init_flag == 0){
	   init_flag = 1;
	   ats_wlt_resp_init();
	}

	if (!memcmp(&buf[index], ATS_CMD_SET_BTEDR_MAC, sizeof(ATS_CMD_SET_BTEDR_MAC)-1)){
		ats_wlt_shell_set_btedr_mac(dev, buf, size);
	}
	else if (!memcmp(&buf[index], ATS_CMD_SET_BTBLE_MAC, sizeof(ATS_CMD_SET_BTBLE_MAC)-1)){
		ats_wlt_shell_set_btble_mac(dev, buf, size);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GET_BTEDR_MAC, sizeof(ATS_CMD_GET_BTEDR_MAC)-1)){
		ats_wlt_shell_get_btedr_mac(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GET_BTBLE_MAC, sizeof(ATS_CMD_GET_BTBLE_MAC)-1)){
		ats_wlt_shell_get_btble_mac(0, 0, 0);
	}
	else if (!memcmp(&buf[index], ATS_CMD_SET_BTEDR_NAME, sizeof(ATS_CMD_SET_BTEDR_NAME)-1)){
		ats_wlt_shell_set_btedr_name(dev, buf, size);
	}
	else if (!memcmp(&buf[index], ATS_CMD_SET_BTBLE_NAME, sizeof(ATS_CMD_SET_BTBLE_NAME)-1)){
		ats_wlt_shell_set_btble_name(dev, buf, size);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GET_BTEDR_NAME, sizeof(ATS_CMD_GET_BTEDR_NAME)-1)){
		ats_wlt_shell_get_btedr_name(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GET_BTBLE_NAME, sizeof(ATS_CMD_GET_BTBLE_NAME)-1)){
		ats_wlt_shell_get_btble_name(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GET_FIRMWARE_VERSION, sizeof(ATS_CMD_GET_FIRMWARE_VERSION)-1)){
		ats_wlt_shell_get_firmware_version(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_GPIO, sizeof(ATS_CMD_GPIO)-1)){
		ats_wlt_shell_gpio_test(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_SET_HARMAN_KEY, sizeof(ATS_CMD_SET_HARMAN_KEY)-1)){
		ats_wlt_shell_harman_key_write(0, 0, 0);
	}
	else if (!memcmp(&buf[index], ATS_CMD_ENTER_SIGNAL, sizeof(ATS_CMD_ENTER_SIGNAL)-1)){
		ats_wlt_shell_enter_signal_test_mode(0, 0, 0);
	}
	else if (!memcmp(&buf[index], ATS_CMD_ENTER_NON_SIGNAL, sizeof(ATS_CMD_ENTER_NON_SIGNAL)-1)){
		ats_wlt_shell_enter_nonsignal_test_mode(0, 0, 0);
	}
	else if (!memcmp(&buf[index], ATS_CMD_ENTER_ADFU, sizeof(ATS_CMD_ENTER_ADFU)-1)){
		ats_wlt_shell_enter_adfu(0, 0, 0);
	}	
	else if (!memcmp(&buf[index], ATS_CMD_DEVICE_RESET, sizeof(ATS_CMD_DEVICE_RESET)-1)){
		ats_wlt_shell_system_reset(0, 0, 0);
	}	
	else{
	    ats_wlt_cmd_response_ok_or_fail(dev, 0);
		goto __exit_exit;
	}
	return 0;

__exit_exit:
    return -1;
}

static int wlt_read_data_handler(struct device *dev)
{
	ats_wlt_uart * ats_uart = &ats_wlt_uart_context;
	int rx_size=0;
	 
	int s1;
	do {
		s1 = stream_read(ats_uart->uio, ats_uart->rx_buffer+rx_size, 1);
		rx_size += s1;
	} while (s1 > 0);

	memcpy(p_ats_info->data_buf, ats_uart->rx_buffer, rx_size);

	if (rx_size == 0){
		return 0;
	}
	if(dev == NULL){
		ats_wlt_write_data("dev == NULL",sizeof("dev == NULL")-1);
		return 0;
	}
	//stream_write(ats_uart->uio, p_ats_info->data_buf, rx_size);
	ats_wlt_command_shell_handler(dev, p_ats_info->data_buf, rx_size);
	return 0;
}

void ats_wlt_write_data(unsigned char *buf, int len)
{
  ats_wlt_uart * ats_uart = &ats_wlt_uart_context;
  stream_write(ats_uart->uio, buf, len);	
}

static void wlt_rx_timer_cb(struct thread_timer *timer, void* pdata)
{
	struct device *dev = (struct device *)pdata;
	wlt_read_data_handler(dev);
}

static void ats_wlt_thread_main_loop(void *p1, void *p2, void *p3)
{
    os_sem *callback_sem = (os_sem *)p1;

	struct device *dev = p_ats_info->ats_uart_dev;
	struct _ats_wlt_thread_msg_t msg = {0};

    p_ats_info->thread_running = 1;

    if (callback_sem){
        os_sem_give(callback_sem);
    }

	if (!dev){
        ats_wlt_write_data("device is NULLt",sizeof("device is NULL")-1);
		goto __thread_exit;
	}

	thread_timer_init(&p_ats_info->rx_timer, wlt_rx_timer_cb, dev);
    thread_timer_start(&p_ats_info->rx_timer, 0, 10);
	
	ats_wlt_write_data("------>enter_wlt_factory succefull!\n",40);
	while (p_ats_info->enabled) 
    {
		if (!k_msgq_get(&p_ats_info->msgq, &msg, thread_timer_next_timeout()))
		{
			SYS_LOG_INF("type %d, cmd %d, value %d\n", msg.type,
				    msg.cmd, msg.value);

			switch (msg.type) 
			{
				case WLT_ATS_ENTER_OK:
					ats_wlt_write_data("------>enter OK!\n",20);
					break;
				case WLT_ATS_EXIT:
					ats_wlt_deinit();
					break;					
				default:
					SYS_LOG_ERR("error: 0x%x\n", msg.type);
					continue;
			}
		}
		thread_timer_handle_expired();
	}

__thread_exit:
	
	SYS_LOG_INF("exit\n");

    p_ats_info->thread_running = 0;
}

int ats_wlt_uart_init(struct device *dev)
{
    ats_wlt_uart * ats_uart = &ats_wlt_uart_context;
    struct uart_stream_param uparam;

    console_input_deinit(dev);
	
    ats_uart->uio_opened = 0;

    uparam.uart_dev_name = "UART_0";
    uparam.uart_baud_rate = CONFIG_UART_ACTS_PORT_0_BAUD_RATE;
    ats_uart->uio = uart_stream_create( &uparam.uart_dev_name);
    if(!ats_uart->uio){
       SYS_LOG_ERR("stream create fail");
       return ats_uart->uio_opened;
    }
    stream_open(ats_uart->uio, MODE_IN_OUT);
    ats_uart->uio_opened = 1;

    return ats_uart->uio_opened;
}

int ats_pre_init(void)
{
    int ret = 0;

    p_ats_var = malloc(sizeof(struct _ats_wlt_var));
    if (p_ats_var == NULL){
        SYS_LOG_ERR("ctx malloc fail\n");
        ret = -1;
        goto err_exit;
    }
    memset(p_ats_var, 0, sizeof(struct _ats_wlt_var));
    p_ats_var->ats_cmd_resp_buf = malloc(ATS_WLT_UART_TX_LEN_MAX);
    if (p_ats_var->ats_cmd_resp_buf == NULL){
        SYS_LOG_ERR("buf malloc fail\n");
        ret = -1;
        goto err_exit;
    }
    memset(p_ats_var->ats_cmd_resp_buf, 0, ATS_WLT_UART_TX_LEN_MAX);
    os_mutex_init(ats_wlt_get_mutex());

    goto exit;

err_exit:
    if (p_ats_var){
        if (p_ats_var->ats_cmd_resp_buf){
            free(p_ats_var->ats_cmd_resp_buf);
            p_ats_var->ats_cmd_resp_buf = NULL;
        }
        free(p_ats_var);
        p_ats_var = NULL;
    }
exit:
    return ret;
}


int ats_wlt_init(void)
{
    int ret = -1;
    os_sem callback_sem = {0};

	int msg_num = 20;
	int msg_size = sizeof(struct _ats_wlt_thread_msg_t);
	SYS_LOG_INF("------>\n");
	if(ats_pre_init()){
		/* pre init fail */
        SYS_LOG_INF("ats_pre_init failed\n");
		goto err_exit;		
	}

	if (p_ats_info)
	{
        SYS_LOG_INF("already init\n");
		return 0;
	}

	os_sem_init(&callback_sem, 0, 1);

	p_ats_info = malloc(sizeof(struct _wlt_driver_ctx_t));
	if (p_ats_info == NULL)
	{
		SYS_LOG_ERR("ctx malloc fail\n");
		goto err_exit;
	}

	memset(p_ats_info, 0, sizeof(struct _wlt_driver_ctx_t));

	p_ats_info->ats_uart_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	if (p_ats_info->ats_uart_dev == NULL)
	{
		SYS_LOG_ERR("uart device not found\n");
		goto err_exit;
	}

	ret = ats_wlt_uart_init(p_ats_info->ats_uart_dev);

    p_ats_info->enabled = true;

	p_ats_info->msg_buf = malloc(msg_num * msg_size);
    if (p_ats_info->msg_buf == NULL)
    {
       ats_wlt_write_data("panlm test5",sizeof("panlm test6")-1);
		goto err_exit;
    }

	memset(p_ats_info->msg_buf, 0, sizeof(msg_num * msg_size));
	k_msgq_init(&p_ats_info->msgq, p_ats_info->msg_buf, msg_size, msg_num);
	p_ats_info->thread_stack = app_mem_malloc(ATS_WLT_THREAD_STACK_SZ);
	if (p_ats_info->thread_stack == NULL)
	{
		ats_wlt_write_data("panlm test7",sizeof("panlm test7")-1);
		goto err_exit;
	}
    k_thread_create(&p_ats_info->thread_data, 
		(k_thread_stack_t)p_ats_info->thread_stack,
        ATS_WLT_THREAD_STACK_SZ,
        (k_thread_entry_t)ats_wlt_thread_main_loop, &callback_sem, NULL, NULL,
        ATS_WLT_THREAD_PRIO, 0, K_NO_WAIT);
    os_sem_take(&callback_sem, K_FOREVER);
    ats_wlt_write_data("panlm test9\n",sizeof("panlm test9")-1);
    return 0;

err_exit:
	if (p_ats_info != NULL){
		if (p_ats_info->msg_buf != NULL){
			free(p_ats_info->msg_buf);
		}
		if (p_ats_info->thread_stack != NULL){
			app_mem_free(p_ats_info->thread_stack);
			p_ats_info->thread_stack = NULL;
		}
		free(p_ats_info);
		p_ats_info = NULL;
	}
	ats_wlt_write_data("live fail",sizeof("live fail")-1);
    return ret;
}

int ats_wlt_deinit(void)
{
    if(p_ats_info != NULL){
        p_ats_info->enabled = false;
		
        while(p_ats_info->thread_running){
            k_sleep(1);
        }
		if (p_ats_info->msg_buf != NULL){
			free(p_ats_info->msg_buf);
		}
		if (p_ats_info->thread_stack != NULL){
			app_mem_free(p_ats_info->thread_stack);
			p_ats_info->thread_stack = NULL;
		}
        free(p_ats_info);
        p_ats_info = NULL;
        SYS_LOG_INF("ok\n");
    }
    else{
        SYS_LOG_INF("already deinit\n");
    }
	trace_dma_print_set(true);//enable system printf
	SYS_LOG_INF("already deinit\n");
    return 0;
}


void ats_wlt_start(void)
{
	ats_wlt_init();
}


#endif


