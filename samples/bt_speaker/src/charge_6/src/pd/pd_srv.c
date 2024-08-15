#include <zephyr.h>
#include <logging/sys_log.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <app_defines.h>
#include <srv_manager.h>
#include <msg_manager.h>
#include <thread_timer.h>
#include <pd_srv.h>
#include <power_manager.h>
#include <wltmcu_manager_supply.h>

extern void battery_status_remaincap_display_handle(uint8_t status, u16_t cap,bool key_flag, int time);
#define PD_SRV_STACK_SIZE    2048
char __aligned(STACK_ALIGN) pdsrv_stack_area[PD_SRV_STACK_SIZE];


static void pd_srv_sync_init_callback(struct app_msg *msg, int result, void *reply)
{
	if (msg->sync_sem) {
		os_sem_give((os_sem *)msg->sync_sem);
	}
}

static void pd_srv_sync_msg_callback(struct app_msg *msg, int result, void *reply)
{
	if (msg->sync_sem) {
		os_sem_give((os_sem *)msg->sync_sem);
	}
}


void pd_srv_sync_init(void)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	printk("pd_srv_sync_init\n");
	srv_manager_active_service(CONFIG_PD_SRV_NAME);
	os_sem_init(&return_notify, 0, 1);
	msg.type = MSG_PD_SRV_INIT;
	msg.value = 0;
	msg.callback = pd_srv_sync_init_callback;
	msg.sync_sem = &return_notify;

	if (!send_async_msg(CONFIG_PD_SRV_NAME, &msg)) {
		return;
	}

	if (os_sem_take(&return_notify, OS_SECONDS(30)) != 0) {
		printk("pd_srv_sync_end timer out\n");
		return;
	}
	printk("pd_srv_sync_end\n");
}

int pd_srv_sync_exit(void)
{
	struct app_msg msg = {0};
	os_sem return_notify;

#ifdef CONFIG_DATA_ANALY
	msg.type = MSG_EXIT_DATA_ANALY;
	msg.cmd = 0;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
#endif	
	os_sem_init(&return_notify, 0, 1);
	msg.type = MSG_PD_SRV_EXIT;
	msg.value = 0;
	msg.callback = pd_srv_sync_init_callback;
	msg.sync_sem = &return_notify;

	if (!send_async_msg(CONFIG_PD_SRV_NAME, &msg)) {
		return -1;
	}

	if (os_sem_take(&return_notify, OS_SECONDS(10)) != 0) {
		return -1;
	}
	
	k_sleep(5*1000);
	//to do if no power dowm
	return -1;
}

int pd_srv_event_notify(int event ,int value)
{
	struct app_msg msg = {0};

	printk("pd_srv_event_notify %d %d\n", event, value);
	msg.type = MSG_PD_SRV_EVENT;
	msg.cmd = event;
	msg.value = value;
	if (!send_async_msg(CONFIG_PD_SRV_NAME, &msg)) {
		return -1;
	}
	return 0;
}


int pd_srv_sync_send_msg(int event, int value)
{

	struct app_msg msg = {0};
	os_sem pd_sem_sync_msg;

	printk("pd_srv_event_send_msg %d %d\n", event, value);

	os_sem_init(&pd_sem_sync_msg, 0, 1);
	msg.type = MSG_PD_SRV_SYNC_MSG;
	msg.cmd = event;
	msg.value = value;
	msg.callback = pd_srv_sync_msg_callback;
	msg.sync_sem = &pd_sem_sync_msg;

	if (!send_async_msg(CONFIG_PD_SRV_NAME, &msg)) {
		return -1;
	}

	if (os_sem_take(&pd_sem_sync_msg, OS_SECONDS(10)) != 0) {
		printk("pd_srv_event_send_msg timerout %d %d\n", event, value);
		return -2;
	}
	
	printk("pd_srv_event_send_msg OK %d %d\n", event, value);
	return 0;
}




extern bool run_mode_is_demo(void);
extern void battery_charging_remaincap_is_full(void);
extern void battery_charging_LED_on_all(void);
extern void battery_remaincap_low_poweroff(void);
extern void power_manager_battery_display_reset(void);
void batt_led_manager_set_display(int led_status)
{

   int temp_status = power_manager_get_charge_status();

   if(temp_status == POWER_SUPPLY_STATUS_DISCHARGE
		   ||temp_status == POWER_SUPPLY_STATUS_FULL
		  )
   	{
	    if(run_mode_is_demo())
	   	{
	    	/// printk("batt_led_manager_set_display led_status = %d\n",led_status);
			 if(pd_manager_get_poweron_filte_battery_led() == WLT_FILTER_DISCHARGE_POWERON
			 	||
power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
	    	 {
	    	    printk("batt_led_manager_set_display led_status = %d\n",led_status);
	    	   led_status = BATT_LED_NORMAL_OFF;
			 }
    	}
   }
  printk("batt_led_manager_set_display led_status = %d\n",led_status);
  switch(led_status)
  	{
    case BATT_LED_NORMAL_ON:
		  battery_charging_LED_on_all();
		break;
    case BATT_LED_NORMAL_OFF: //all off
		 battery_charging_remaincap_is_full();
        break;
	case REMOVE_CHARGING_WARNING:
    case REBOOT_LED_STATUS:
		bt_ui_charging_warning_handle(0);
		break;
	case CHARGING_WARNING:
		bt_ui_charging_warning_handle(5);
		break;
	case LOW_POWER_OFF_LED_STATUS:
		battery_remaincap_low_poweroff();
		break;
    case BATT_LED_CAHARGING:
	case BATT_LED_ON_2S:	
    case BATT_LED_ON_10S:
	case BATT_LED_EXT_CAHARGE:	
	case BATT_PWR_LED_ON_0_5S:
		power_manager_battery_display_handle(led_status,1);
		break;
    case DOME_MODE_LED_STATUS:
		 battery_charging_LED_on_all();
		break;

	case BATT_LED_RESET:
		power_manager_battery_display_reset();
		break;
	default:
			break;	
    }

}

void pd_srv_charger_init(void)
{
	int prio = os_thread_priority_get(os_current_get());
	os_thread_priority_set(os_current_get(), -1);		
	pd_manager_init(true);
	os_thread_priority_set(os_current_get(), prio);
}

extern void io_expend_aw9523b_ctl_20v5_set(uint8_t onoff);

void pd_srv_sync_msg_process(struct app_msg *msg)
{
	printk("pd srv sync msg %d %d\n",msg->cmd,msg->value);

	switch (msg->cmd){
		case PA_EVENT_AW_20V5_CTL:
			io_expend_aw9523b_ctl_20v5_set(msg->value);
		break;


		default:
		break;
	}



}

void pd_srv_event_process(struct app_msg *msg)
{
	printk("pd srv event %d %d\n",msg->cmd,msg->value);
	switch (msg->cmd){
		case PD_EVENT_SOURCE_REFREST:
			pd_set_source_refrest();
		break;

		case PD_EVENT_SOURCE_BATTERY_DISPLAY:
			 batt_led_manager_set_display(msg->value);
			#if 0
			if(msg->value == 2)
				battery_status_remaincap_display_handle(POWER_SUPPLY_STATUS_FULL,100,0,POWER_MANAGER_BATTER_10_SECOUND);
			else if(msg->value == 3)
				 battery_status_remaincap_display_handle(POWER_SUPPLY_STATUS_FULL,power_manager_get_battery_capacity(),0,POWER_MANAGER_BATTER_10_SECOUND);
			else if(msg->value == 4)
				  power_manager_battery_display_handle(1, POWER_MANAGER_BATTER_2_SECOUND);
			else
			      power_manager_battery_display_handle(msg->value, POWER_MANAGER_BATTER_10_SECOUND);
			#endif
		break;

		case PD_EVENT_BT_LED_DISPLAY:
			bt_manager_sys_event_led_display(msg->value);
		break;

		case PD_EVENT_AC_LED_DISPLAY:
			bt_manager_auracast_led_display(msg->value); 
		break;

		case PD_EVENT_MUC_UPDATA:
			bt_manager_user_ctl_mcu_update();
		break;

		case PD_EVENT_LED_LOCK:
			mcu_ui_set_led_enable_state(msg->value);
		break;


		default:
			break;
	}

}



void pd_service_main_loop(void * parama1, void * parama2, void * parama3)
{
	struct app_msg msg = { 0 };
	bool terminated = false;
	printk("pd_service_main_loop start\n");
	while (!terminated) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {

			SYS_LOG_INF("msg: type:%d\n", msg.type);

			switch (msg.type) {
				case MSG_PD_SRV_INIT:
					pd_srv_charger_init();
					break;
				case MSG_PD_SRV_EXIT:
					pd_manager_deinit(0);
					break;

				case MSG_PD_SRV_EVENT:
				{
					pd_srv_event_process(&msg);
					break;
				}

				case MSG_PD_SRV_SYNC_MSG:
					pd_srv_sync_msg_process(&msg);
					break;

				default:
					break;

			}
			if (msg.callback)
				msg.callback(&msg, 0, NULL);

		}
		thread_timer_handle_expired();

	}
}

SERVICE_DEFINE(pd_srv,pdsrv_stack_area, PD_SRV_STACK_SIZE,11, BACKGROUND_APP,\
				NULL, NULL, NULL, \
				pd_service_main_loop);
