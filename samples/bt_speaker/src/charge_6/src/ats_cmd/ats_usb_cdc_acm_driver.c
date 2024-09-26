#include <zephyr.h>
#include <device.h>
#include <usb_cdc.h>
#include <device.h>
#include <os_common_api.h>
#include <uart.h>
#include <string.h>
#include <msg_manager.h>
#include <thread_timer.h>
#include "ats_cmd/ats.h"
#include <mem_manager.h>
#include <kernel.h>
#include <stream.h>
#include <uart_stream.h>


#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ats_cdc_driver"
#include <logging/sys_log.h>

typedef struct ats_uart {
    io_stream_t uio;
    u8_t uio_opened;
  //  char rx_buffer[600];
} ats_uart_t;

ats_uart_t ats_uart_context;


#define CDC_ACM_THREAD_STACK_SZ	2048
#define CDC_ACM_THREAD_PRIO	5

extern int trace_print_disable_set(unsigned int print_disable);
struct _ats_usb_cdc_acm_driver_ctx_t {
	struct device *usb_cdc_acm_dev;

 	struct k_thread cdc_acm_thread_data;
	u8_t *cdc_acm_thread_stack;

	bool cdc_acm_enabled;
	
	u8_t thread_running;

	struct k_msgq msgq;
	char *msg_buf;

	struct thread_timer rx_timer;

	//char data_buf[600];
};
char data_buf[600], rx_buffer[600];

static struct _ats_usb_cdc_acm_driver_ctx_t *p_ctx;

struct k_msgq *get_ats_usb_cdc_acm_thread_msgq(void)
{
	return &p_ctx->msgq;
}

static volatile bool data_transmitted;
static volatile bool data_arrived;

static int read_data_handler(struct device *dev)
{
	ats_uart_t * ats_uart = &ats_uart_context;
	int rx_size=0;
	 
	   int s1;
	   do {
		   s1 = stream_read(ats_uart->uio, rx_buffer+rx_size, 1);
		   rx_size += s1;
	   } while (s1 > 0);

         memcpy(p_ctx->data_buf, ats_uart->rx_buffer, rx_size);

		if (rx_size == 0)
		{
			// ats_usb_cdc_acm_write_data("live rx_size != 60",sizeof("live rx_size != 60")-1);
			 return 0;
		
		}
        //stream_write(ats_uart->uio, p_ctx->data_buf, rx_size);
		

	//	print_buffer(p_ctx->data_buf, 1, rx_size, 16, 0);
		
      if(dev == NULL)
     	{
			 ats_usb_cdc_acm_write_data("dev == NULL",sizeof("dev == NULL")-1);
			 return 0;
		
		}
		ats_usb_cdc_acm_shell_command_handler(dev, p_ctx->data_buf, rx_size);
	return 0;
}

void ats_usb_cdc_acm_write_data(unsigned char *buf, int len)
{
  ats_uart_t * ats_uart = &ats_uart_context;

  stream_write(ats_uart->uio, buf, len);
		
}

static void rx_timer_cb(struct thread_timer *timer, void* pdata)
{
	struct device *dev = (struct device *)pdata;
	//ats_usb_cdc_acm_write_data("live rx_timer_cb",sizeof("live rx_timer_cb")-1);

	read_data_handler(dev);
}

static void cdc_acm_thread_main_loop(void *p1, void *p2, void *p3)
{
    os_sem *callback_sem = (os_sem *)p1;
	char buffer[2+1] = "00";
	struct device *dev = p_ctx->usb_cdc_acm_dev;
	struct _ats_usb_cdc_acm_thread_msg_t msg = {0};

    p_ctx->thread_running = 1;

    if (callback_sem)
    {
        os_sem_give(callback_sem);
    }

	if (!dev) 
    {
        ats_usb_cdc_acm_write_data("device is NULLt",sizeof("device is NULL")-1);
		goto __thread_exit;
	}


	thread_timer_init(&p_ctx->rx_timer, rx_timer_cb, dev);
    thread_timer_start(&p_ctx->rx_timer, 0, 10);


	while (p_ctx->cdc_acm_enabled) 
    {
		if (!k_msgq_get(&p_ctx->msgq, &msg, thread_timer_next_timeout()))
		{
		  	//ats_usb_cdc_acm_write_data("key1 test",sizeof("key1 test")-1);
			SYS_LOG_INF("type %d, cmd %d, value %d\n", msg.type,
				    msg.cmd, msg.value);

			switch (msg.type) 
			{
				case 1:
					ats_usb_cdc_acm_key_check_report(dev, msg.value);
					break;
				case 10:
					buffer[1] = '0';
					ats_usb_cdc_acm_cmd_response_at_data(
					dev, ATS_CMD_RESP_ENTER_STANDBY, sizeof(ATS_CMD_RESP_ENTER_STANDBY)-1, 
					buffer, sizeof(buffer)-1);
					break;				
				case 11:
					buffer[1] = '1';
					ats_usb_cdc_acm_cmd_response_at_data(
					dev, ATS_CMD_RESP_ENTER_STANDBY, sizeof(ATS_CMD_RESP_ENTER_STANDBY)-1, 
					buffer, sizeof(buffer)-1);
					break;	
				case 12:
					buffer[1] = '2';
					ats_usb_cdc_acm_cmd_response_at_data(
					dev, ATS_CMD_RESP_ENTER_STANDBY, sizeof(ATS_CMD_RESP_ENTER_STANDBY)-1, 
					buffer, sizeof(buffer)-1);
					break;						
				default:
					SYS_LOG_ERR("error: 0x%x\n", msg.type);
					continue;
			}
		}
		thread_timer_handle_expired();
	}

__thread_exit:
	if (thread_timer_is_running(&p_ctx->rx_timer))
	{
		thread_timer_stop(&p_ctx->rx_timer);
	}
	
	SYS_LOG_INF("exit\n");

    p_ctx->thread_running = 0;
}
extern void console_input_deinit(struct device *dev);
extern void console_input_init(struct device *dev);

extern struct device *uart_console_dev;

int ats_uart_init(struct device *dev)
{
    ats_uart_t * ats_uart = &ats_uart_context;
    struct uart_stream_param uparam;
	//SYS_LOG_INF();

	//disable uart0 tx dma print
	// trace_print_disable_set(true);
    console_input_deinit(dev);
	k_sleep(2);

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

int ats_uart_deinit(struct device *dev)
{
    ats_uart_t * ats_uart = &ats_uart_context;
	//SYS_LOG_INF();

	//disable uart0 tx dma print
    stream_close(ats_uart->uio);
	//stream_destroy(ats_uart->uio);
	ats_uart->uio_opened = 0;
	//k_sleep(2);
    //console_input_init(dev);
	if (thread_timer_is_running(&p_ctx->rx_timer))
	{
		thread_timer_stop(&p_ctx->rx_timer);
	}  

	trace_print_disable_set(false);
   
   // stream_write(ats_uart->uio, ATS_CMD_RESP_OK, (sizeof(ATS_CMD_RESP_OK) - 1));
    return ats_uart->uio_opened;
}

int ats_usb_cdc_acm_init(void)
{
    int ret = -1;
    os_sem callback_sem = {0};

	int msg_num = 20;
	int msg_size = sizeof(struct _ats_usb_cdc_acm_thread_msg_t);

	if (p_ctx)
	{
        SYS_LOG_INF("already init\n");
		return 0;
	}

	os_sem_init(&callback_sem, 0, 1);

	p_ctx = malloc(sizeof(struct _ats_usb_cdc_acm_driver_ctx_t));
	if (p_ctx == NULL)
	{
		SYS_LOG_ERR("ctx malloc fail\n");
		goto err_exit;
	}

	memset(p_ctx, 0, sizeof(struct _ats_usb_cdc_acm_driver_ctx_t));

    	p_ctx->usb_cdc_acm_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	   if (p_ctx->usb_cdc_acm_dev == NULL)
	   {
		   SYS_LOG_ERR("uart device not found\n");
		   goto err_exit;
	   }

       ret = ats_uart_init(p_ctx->usb_cdc_acm_dev);
	   k_sleep(2);
	   ats_usb_cdc_acm_write_data("live test",sizeof("live test")-1);


    p_ctx->cdc_acm_enabled = true;

	p_ctx->msg_buf = malloc(msg_num * msg_size);
    if (p_ctx->msg_buf == NULL)
    {
       ats_usb_cdc_acm_write_data("live test5",sizeof("live test6")-1);
		goto err_exit;
    }

	memset(p_ctx->msg_buf, 0, sizeof(msg_num * msg_size));

	k_msgq_init(&p_ctx->msgq, p_ctx->msg_buf, msg_size, msg_num);

	p_ctx->cdc_acm_thread_stack = app_mem_malloc(CDC_ACM_THREAD_STACK_SZ);
	if (p_ctx->cdc_acm_thread_stack == NULL)
	{
		ats_usb_cdc_acm_write_data("live test7",sizeof("live test7")-1);
		goto err_exit;
	}
	//ats_usb_cdc_acm_write_data("live test8",sizeof("live test8")-1);

    k_thread_create(&p_ctx->cdc_acm_thread_data, 
		(k_thread_stack_t)p_ctx->cdc_acm_thread_stack,
        CDC_ACM_THREAD_STACK_SZ,
        (k_thread_entry_t)cdc_acm_thread_main_loop, &callback_sem, NULL, NULL,
        CDC_ACM_THREAD_PRIO, 0, K_NO_WAIT);

    os_sem_take(&callback_sem, K_FOREVER);

    ats_usb_cdc_acm_write_data("live test9",sizeof("live test9")-1);
	ats_set_enable(true);
    return 0;

err_exit:
	if (p_ctx != NULL)
	{
		usb_cdc_acm_exit();

		if (p_ctx->msg_buf != NULL)
		{
			free(p_ctx->msg_buf);
		}

		if (p_ctx->cdc_acm_thread_stack != NULL)
		{
			app_mem_free(p_ctx->cdc_acm_thread_stack);
			p_ctx->cdc_acm_thread_stack = NULL;
		}

		free(p_ctx);
		p_ctx = NULL;
	}
	ats_usb_cdc_acm_write_data("live fail",sizeof("live fail")-1);

    return ret;
}

int ats_usb_cdc_acm_deinit(void)
{
    if (p_ctx != NULL)
    {
       
	   
        p_ctx->cdc_acm_enabled = false;

        while(p_ctx->thread_running)
        {
            k_sleep(1);
        }

        usb_cdc_acm_exit();

		if (p_ctx->msg_buf != NULL)
		{
			free(p_ctx->msg_buf);
		}

		if (p_ctx->cdc_acm_thread_stack != NULL)
		{
			app_mem_free(p_ctx->cdc_acm_thread_stack);
			p_ctx->cdc_acm_thread_stack = NULL;
		}

        free(p_ctx);
        p_ctx = NULL;
		
          ats_uart_deinit(p_ctx->usb_cdc_acm_dev);
        SYS_LOG_INF("ok\n");
    }
    else
    {
        SYS_LOG_INF("already deinit\n");
    }
	ats_set_enable(false);
    return 0;
}
