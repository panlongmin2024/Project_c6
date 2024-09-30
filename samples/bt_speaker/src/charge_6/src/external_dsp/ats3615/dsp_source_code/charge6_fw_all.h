const unsigned int charge6_fw[] = 
{
	//0,0
	#include "charge6.inc"
};

const __aligned(4) unsigned char charge6_3nod[] = 
{
	#include "tuning_3nod_5_1.inc"
};
unsigned int charge6_3nod_size = sizeof(charge6_3nod);







const __aligned(4) unsigned char charge6_ggec[] = 
{
	#include "tuning_Charge6_S09_T27_GGEC_3_1_DV2_20240922_v2.inc"
};
unsigned int charge6_ggec_size = sizeof(charge6_ggec);


const __aligned(4) unsigned char charge6_ggec_PB[] = 
{
	#include "tuning_Charge6_S09_T27_GGEC_3_1_DV2_20240922_v3_PTB.inc"
};
unsigned int charge6_ggec_PB_size = sizeof(charge6_ggec_PB);








const __aligned(4) unsigned char charge6_tcl[] = 
{
	#include "tuning_Charge6_S09_T27_TCL_1_1_DV2_20240922_v2.inc"
};
unsigned int charge6_tcl_size = sizeof(charge6_tcl);

const __aligned(4) unsigned char charge6_tcl_PB[] = 
{
	#include "tuning_Charge6_S09_T27_TCL_1_1_DV2_20240922_v3_PTB.inc"
};
unsigned int charge6_tcl_PB_size = sizeof(charge6_tcl_PB);