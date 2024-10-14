
#ifndef PD_MANAGER_H
#define PD_MANAGER_H



/** pd event */
enum pd_event_e
{
	/** ps event low battery, need power off tone */
	PD_EVENT_LOW_BATTERY = 1,
	/** pd event water alart stop, No need power off tone */
	PD_EVENT_WATER_WARNNING_STOP = 2,
	/** pd event poweroff, no need power off tone */
	PD_EVENT_POWER_OFF,
    /** pd event poweroff, no need power off tone */
    PD_EVENT_OTG_MOBILE_POWER_OFF,

    /** pd event poweroff, no need power off tone */
	PD_EVENT_SOURCE_LOW_BATTERY,
	
	PD_EVENT_SOURCE_EXIT_ATS,

	PD_EVENT_SOURCE_BAT_VERIFY_FAIL_POWEROFF,
};

enum pd_water_warning_valve
{
	PD_5V_VALVE = 0,
	PD_9V_VALVE,
	PD_12V_VALVE,
	PD_20V_VALVE,
};
#endif