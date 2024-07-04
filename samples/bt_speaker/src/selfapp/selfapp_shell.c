#include <os_common_api.h>
#include <stdio.h>
#include <init.h>
#include <string.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include "selfapp_internal.h"
#include "selfapp_api.h"

static int shell_set_hw_test(int argc, char *argv[])
{
	int en = 1;

	if (argc == 2) {
		en = strtoul(argv[1], (char **)NULL, 10);
	} 

	selfapp_eq_cmd_switch_hw_test((en==0)?false:true);

	return 0;

}

static int shell_set_bat_capacity(int argc, char *argv[])
{
	int bat = 100;
	if (argc == 2) {
		bat = strtoul(argv[1], (char **)NULL, 10);
	}

	selfapp_set_user_bat_cap(bat);

	return 0;
}

static const struct shell_cmd app_commands[] = {
	{"hwtest", shell_set_hw_test, "hwtest 0/1. Disable/Enable DSP hareware test mode."},
	{"bat", shell_set_bat_capacity, "set bat capacity."},
	{NULL, NULL, NULL}};

SHELL_REGISTER("selfapp", app_commands);
