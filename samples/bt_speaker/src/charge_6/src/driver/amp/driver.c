#include <zephyr.h>
#include <device.h>
#include <soc_regs.h>
#include <os_common_api.h>
#include <gpio.h>
#include "wltmcu_manager_supply.h"
#include <thread_timer.h>

#ifdef CONFIG_C_AMP_AW85828
extern int aw85xxx_pa_start(void);
extern int _aw85xxx_pa_stop(void);
extern void aw85xxx_init();
extern int aw85xxx_pa_stop(void);
#endif
#ifdef CONFIG_C_AMP_TAS5828M
extern int amp_tas5828m_registers_init(void);
extern int amp_tas5828m_registers_deinit(void);
extern int amp_tas5828m_clear_fault(void);
extern int amp_tas5828m_pa_stop(void);
extern int amp_tas5828m_pa_start(void);
extern int amp_tas5828m_pa_select_left_speaker(void);
extern int amp_tas5828m_pa_select_right_speaker(void);
#endif

static struct thread_timer amp_timer;
static int pa_mute_state = 0;

void hm_ext_pa_stop(void)
{
    if(pa_mute_state == 0){
        printk("pa mute\n");
        pa_mute_state = 1;
        if(ReadODM() == HW_GUOGUANG_BOARD){
            #ifdef CONFIG_C_AMP_AW85828
            _aw85xxx_pa_stop();
            #endif
        }	
        else{
            #ifdef CONFIG_C_AMP_TAS5828M
            //amp_tas5828m_registers_deinit();
            amp_tas5828m_pa_stop();    

            #endif	
        }		
    }
}

void hm_ext_pa_start(void)
{
	if(pa_mute_state == 1){
         printk("pa unmute\n");
         pa_mute_state = 0;
        if(ReadODM() == HW_GUOGUANG_BOARD)
        {
            #ifdef CONFIG_C_AMP_AW85828
            aw85xxx_pa_start();
            #endif
        }else{
            //amp_tas5828m_registers_init();
            amp_tas5828m_pa_start();
        }
    }
}

void hm_ext_pa_deinit(void)
{
    uint8_t hw_info = ReadODM();
    if(hw_info == HW_GUOGUANG_BOARD)
    {
        #ifdef CONFIG_C_AMP_AW85828
        aw85xxx_pa_stop();
        #endif
    }else{
        #ifdef CONFIG_C_AMP_TAS5828M
        amp_tas5828m_registers_deinit();
        #endif	
    }	
}

void hm_ext_pa_init(void)
{
    if(ReadODM() == HW_GUOGUANG_BOARD)
	{
		aw85xxx_init();
	}else{
		amp_tas5828m_registers_init();
	}	

}

/* for factory test */
void hm_ext_pa_select_left_speaker(void)
{
	printk("------>hm_ext_pa_select_left_speaker\n");
	if(ReadODM() == HW_GUOGUANG_BOARD)
	{
		#ifdef CONFIG_C_AMP_AW85828
			int aw85xxx_select_left_speaker(void);
			aw85xxx_select_left_speaker();
		#endif
	}else{
		#ifdef CONFIG_C_AMP_TAS5828M
            amp_tas5828m_registers_init();//first open amp
			amp_tas5828m_pa_select_left_speaker();
		#endif	
	}
}
/* for factory test */
void hm_ext_pa_select_right_speaker(void)
{
	printk("------>hm_ext_pa_select_right_speaker\n");
	if(ReadODM() == HW_GUOGUANG_BOARD)
	{
		#ifdef CONFIG_C_AMP_AW85828
			int aw85xxx_select_right_speaker(void);
			aw85xxx_select_right_speaker();
		#endif
	}else{
		#ifdef CONFIG_C_AMP_TAS5828M
            amp_tas5828m_registers_init();//first open amp
			amp_tas5828m_pa_select_right_speaker();
		#endif	
	}
}

static void hm_ext_pa_clear_fault_timer_handle(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct device *gpio_dev = NULL;
	u32_t value;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("hm_ext_pa_clear_fault_timer_handle io not find\n");
		return ;
	}

	gpio_pin_configure(gpio_dev, 14, GPIO_DIR_IN);
	gpio_pin_read(gpio_dev, 14, &value);
	if(value == 0){
        printk("err hm_ext_pa_clear_fault_timer_handle pa fault\n");
        if(ReadODM() == HW_GUOGUANG_BOARD)
        {
            
        }else{
            amp_tas5828m_clear_fault();
            // amp_tas5828m_pa_start();
        }	
	}
}

void amp_tas5828m_clear_fault_timer_init(void)
{
    thread_timer_init(&amp_timer, hm_ext_pa_clear_fault_timer_handle, NULL);
    thread_timer_start(&amp_timer, 2000, 2000);
}
