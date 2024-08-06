
#include "ats_wlt_factory.h"

#ifdef CONFIG_WLT_ATS_ENABLE
static bool isWltAtsMode_readIO = false;
//static bool isWltAtsMode_comm = false;

ats_wlt_uart ats_wlt_uart_enter;
static void ats_wlt_enter_write_data(unsigned char *buf, int len);

static void ats_wlt_enter_success(void)
{
	
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

static int ats_wlt_enter_comm_data_handler(struct device *dev)
{
	ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
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
		ats_wlt_enter_write_data("dev == NULL",sizeof("dev == NULL")-1);
		return 0;
	}
	stream_write(ats_uart->uio, p_ats_info->data_buf, rx_size);
	ats_wlt_command_handler(dev, p_ats_info->data_buf, rx_size);
	return 0;
}

static void ats_wlt_enter_write_data(unsigned char *buf, int len)
{
  ats_wlt_uart * ats_uart = &ats_wlt_uart_enter;
  stream_write(ats_uart->uio, buf, len);	
}
/* check wlt ats enter? */
static void ats_wlt_enter_timer_cb(struct thread_timer *timer, void* pdata)
{
	struct device *dev = (struct device *)pdata;
	ats_wlt_enter_comm_data_handler(dev);
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

void ats_wlt_enter(void)
{
	SYS_LOG_INF("check wlt ats !\n");
	uint8_t ReadODM(void);
	if(ReadODM() == 1){
		k_sleep(20);
		if(ReadODM() == 1){
			/* is wlt factory test ! */

			isWltAtsMode_readIO = true;
			SYS_LOG_INF("real enter wlt ats !\n");

			/* init uart! */
			ats_wlt_enter_uart_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
			if (ats_wlt_enter_uart_dev == NULL)
			{
				SYS_LOG_ERR("uart device not found\n");
				return;
			}
			ats_wlt_enter_uart_init(ats_wlt_enter_uart_dev);
			
			thread_timer_init(&enter_ats_wlt_timer, ats_wlt_enter_timer_cb, ats_wlt_enter_uart_dev);
		    thread_timer_start(&enter_ats_wlt_timer, 0, 10);			
		}
	}
}
bool get_enter_wlt_ats_state(void)
{
	SYS_LOG_INF("check wlt ats ! isWltAtsMode_readIO %d\n",isWltAtsMode_readIO);
	return isWltAtsMode_readIO;
}

#endif


