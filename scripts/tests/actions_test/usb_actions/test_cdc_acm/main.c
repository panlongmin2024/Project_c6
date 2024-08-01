/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample app for CDC ACM class driver
 *
 * Sample app for USB CDC ACM class driver. The received data is echoed back
 * to the serial port.
 */

#include <stdio.h>
#include <string.h>
#include <device.h>
#include <uart.h>
#include <zephyr.h>
#include <stdio.h>
#include <greatest_api.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_MAIN

static const char *banner1 = "Send characters to the UART device\r\n";
static const char *banner2 = "Characters read:\r\n";
static char version_str[] = "00020002";


static volatile bool data_transmitted;
static volatile bool data_arrived;
static	char data_buf[64];

#define TEST_DAT_LEN   0x400
int cdc_acm_dma_send_init(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data);
/*static */int cdc_acm_dma_send(struct device *dev, char *s, int len);    
/*static */int cdc_acm_dma_send_complete(struct device *dev);     
/*static */int cdc_acm_dma_send_stop(struct device *dev);
extern void *malloc(unsigned int num_bytes);
extern void free(void *ptr);


extern int usb_cdc_acm_init(struct device *dev);

static void interrupt_handler(struct device *dev)
{
	uart_irq_update(dev);

	if (uart_irq_tx_ready(dev)) {
		data_transmitted = true;
	}

	if (uart_irq_rx_ready(dev)) {
		data_arrived = true;
	}
}

static void write_data(struct device *dev, const char *buf, int len)
{
	uart_irq_tx_enable(dev);

	while (len) {
		int written;

		data_transmitted = false;
		written = uart_fifo_fill(dev, (const u8_t *)buf, len);
		while (data_transmitted == false) {
			k_yield();
		}

		len -= written;
		buf += written;
	}

	uart_irq_tx_disable(dev);
}

static int command_handler(struct device *dev, char *str, int len)
{
	/* end with ENTER */
	/*if (str[len - 1] != 0x0D) {
		printf("0x%x\n", str[len - 1]);
		return -EINVAL;
	}*/
    printk("data_buf: %s\n", (u8_t*)data_buf);
	if (!memcmp(data_buf, "AT+WSSW", 7)) {
		write_data(dev, version_str, strlen(version_str));
		return 0;
	}

	/* Not Implemented */
	return -EINVAL;
}


static void read_and_echo_data(struct device *dev, int *bytes_read)
{
	while (data_arrived == false)
		;

	data_arrived = false;

	/* Read all data and echo it back */
	while ((*bytes_read = uart_fifo_read(dev,
	    (u8_t *)data_buf, sizeof(data_buf)))) {
		write_data(dev, data_buf, *bytes_read);
        /* handle */
		command_handler(dev, data_buf, *bytes_read);
	}
}

void dma_send_isr(struct device *dev, u32_t priv_data, int reson)
{
    if (reson == DMA_IRQ_HF){
        ACT_LOG_ID_INF(ALF_STR_dma_send_isr__HFN, 0);
    }else{
        ACT_LOG_ID_INF(ALF_STR_dma_send_isr__TCN, 0);
    }
}



static void test_dma_transport_interface(struct device *dev)
{
    u8_t* dat_start_addr = NULL;
    u32_t i = 0;
    u8_t j = 0;

    dat_start_addr = (u8_t*)malloc(TEST_DAT_LEN);
    if (!dat_start_addr)
    {
        ACT_LOG_ID_INF(ALF_STR_test_dma_transport_interface__TEST_DAT_MALLOC_FAIL, 0);
    }

    for (i = 0; i < TEST_DAT_LEN / 16; i++)
    {
        for(j = 0; j < 16; j++)
        {
            dat_start_addr[16 * i + j] = i;
        }
        /*if (i <= 0xff)
        {
            dat_start_addr[i] = i;
        }
        else if ((i > 0xff) && (i < TEST_DAT_LEN))
        {
            if (((i - 0xff) * 2) <= 0xfe)
            {
                dat_start_addr[i] = (i - 0xff) * 2;
            }
            else
            {
                dat_start_addr[i] = ((i - 0xff) * 2) + 1;
            }
        }*/
    }

    ACT_LOG_ID_INF(ALF_STR_test_dma_transport_interface__ORI_DATN, 0);
    for (i = 0; i < TEST_DAT_LEN; i++)
    {
        if ((i % 16 == 0) && ( i != 0))
        {
            ACT_LOG_ID_INF(ALF_STR_test_dma_transport_interface__N, 0);
        }
        ACT_LOG_ID_INF(ALF_STR_test_dma_transport_interface__D_, 1, dat_start_addr[i]);
        
    }

    ACT_LOG_ID_INF(ALF_STR_test_dma_transport_interface__NORI_DAT_ENDN, 0);
    
    cdc_acm_dma_send_init(dev, CONFIG_UART_CONSOLE_DMA_CHANNEL, \
            dma_send_isr, NULL);

    cdc_acm_dma_send(dev, dat_start_addr, TEST_DAT_LEN);


    //将缓存数据全部输出
    while(1){
        //;
        //#if 0
        if (cdc_acm_dma_send_complete(dev)){
           //trace_stop_tx(trace_ctx);
           cdc_acm_dma_send(dev, dat_start_addr, TEST_DAT_LEN);
        }else{
           continue;
        }
        //#endif
        /*if(!trace_start_tx(trace_ctx)){
            break;
        }*/
    }

    free(dat_start_addr);
    dat_start_addr = NULL;
}

void test_usb_cdc_acm(void)
{
	struct device *dev;
	u32_t baudrate, bytes_read, rts = 0;
	int ret;

    printf("test_usb_cdc_acm>>>>>>>>>>>>>>>>>\n");

    
	dev = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);
	if (!dev) {
		printf("CDC ACM device not found\n");
		return;
	}

    ret = usb_cdc_acm_init(dev);
	if (ret) {
		ACT_LOG_ID_INF(ALF_STR_test_usb_cdc_acm__FAILED_TO_INIT_CDC_A, 0);
		return;
	}

	printf("Wait for DTR\n");
	/*while (1) {
		uart_line_ctrl_get(dev, LINE_CTRL_DTR, &dtr);
		if (dtr)
			break;
	}*/

   while (1) {
		uart_line_ctrl_get(dev, LINE_CTRL_RTS, &rts);
		if (rts)
			break;
	}
	printf("RTS set, start test\n");

	/* They are optional, we use them to test the interrupt endpoint */
	ret = uart_line_ctrl_set(dev, LINE_CTRL_DCD, 1);
	if (ret)
		printf("Failed to set DCD, ret code %d\n", ret);

	ret = uart_line_ctrl_set(dev, LINE_CTRL_DSR, 1);
	if (ret)
		printf("Failed to set DSR, ret code %d\n", ret);

	/* Wait 1 sec for the host to do all settings */
	k_busy_wait(1000000);

	ret = uart_line_ctrl_get(dev, LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret)
		printf("Failed to get baudrate, ret code %d\n", ret);
	else
		printf("Baudrate detected: %d\n", baudrate);

	uart_irq_callback_set(dev, interrupt_handler);
	write_data(dev, banner1, strlen(banner1));
	write_data(dev, banner2, strlen(banner2));

	/* Enable rx interrupts */
	uart_irq_rx_enable(dev);

    test_dma_transport_interface(dev);

	/* Echo the received data */
	while (1) {
		read_and_echo_data(dev, (int *) &bytes_read);
	}
}

TEST tts_usb_cdc_acm(void)
{
    test_usb_cdc_acm();
}

RUN_TEST_CASE("test_usbcdcacm")
{
    RUN_TEST(tts_usb_cdc_acm);
}

