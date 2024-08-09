
#include "ats_wlt_factory.h"

#ifdef CONFIG_WLT_ATS_ENABLE
static bool isWltAtsMode = false;
static struct _wlt_driver_ctx_t *p_ats_wlt_info;

ats_wlt_uart ats_wlt_uart_enter;
static void ats_wlt_enter_write_data(unsigned char *buf, int len);
extern void console_input_deinit(struct device *dev);
static void ats_wlt_enter_write_data(unsigned char *buf, int len);
extern uint8_t ReadODM(void);
static void ats_wlt_enter_success(struct device *dev, u8_t *buf, int len)
{
	//void mcu_ui_power_hold_fn(void);
	//mcu_ui_power_hold_fn();
	ats_wlt_enter_write_data(ATS_SEND_ENTER_WLT_ATS_ACK,sizeof(ATS_SEND_ENTER_WLT_ATS_ACK)-1);
	isWltAtsMode = true;
}

static int ats_wlt_command_handler(struct device *dev, u8_t *buf, int size)
{
	int index = 0;

	if (!memcmp(&buf[index], ATS_AT_CMD_ENTER_WLT_ATS, sizeof(ATS_AT_CMD_ENTER_WLT_ATS)-1)){
		ats_wlt_enter_success(dev, buf, size);
	}
	else{

	}
	return 0;
}

/*static int ats_wlt_enter_comm_data_handler(struct device *dev)
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
*/
static void ats_wlt_enter_write_data(unsigned char *buf, int len)
{
  ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
  stream_write(ats_uart->uio, buf, len);	
}
/* check wlt ats enter? */
/*static void ats_wlt_enter_timer_cb(struct thread_timer *timer, void* pdata)
{
	struct device *dev = (struct device *)pdata;
	ats_wlt_enter_comm_data_handler(dev);
}*/

/*static int ats_wlt_enter_uart_init(struct device *dev)
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
}*/
/* wait communicate from PC! */
/*static int ats_wlt_wait_comm(struct device *dev)
{
	int ret = -1;
	int times = 100;
	while(times--){
		ats_wlt_enter_write_data(ATS_SEND_ENTER_WLT_ATS,sizeof(ATS_SEND_ENTER_WLT_ATS)-1);
	
		ats_wlt_enter_comm_data_handler(dev);
		if(isWltAtsMode){
			break;
		}
		k_sleep(20);
	}
	return ret;
}*/

int ats_wlt_enter(void)
{
	int ret = -1;
	SYS_LOG_INF("check wlt ats ! odm=%d\n",ReadODM());
	
	if(ReadODM() == 0){
		k_sleep(20);
		if(ReadODM() == 0){
			/* is wlt factory test ! */
			
			SYS_LOG_INF("real entering wlt ats !\n");
#if 0
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
			//console_input_deinit(p_ats_wlt_info->ats_uart_dev);
#else
			isWltAtsMode = true;
#endif			
		}
	}
	return ret;
}


int ats_wlt_check_adfu(void)
{
	u32_t value;
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
    gpio_pin_configure(gpio_dev, 2, GPIO_DIR_IN | GPIO_POL_NORMAL);
    gpio_pin_read(gpio_dev, 2, &value);	
	SYS_LOG_INF("------>  value=0x%x\n",value);
	if(!value){
		sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	}
	return 0;
}

bool get_enter_wlt_ats_state(void)
{
	SYS_LOG_INF("check wlt ats ! isWltAtsMode %d\n",isWltAtsMode);
	return isWltAtsMode;
}

#endif


