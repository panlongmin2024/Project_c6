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
	#include "tuning_ggec_3_1.inc"
};
unsigned int charge6_ggec_size = sizeof(charge6_ggec);


const __aligned(4) unsigned char charge6_ggec_PB[] = 
{
	#include "tuning_S08_T26_GGEC_3_1_DV1_20240919_v2_PTB.inc"
};
unsigned int charge6_ggec_PB_size = sizeof(charge6_ggec_PB);








const __aligned(4) unsigned char charge6_tcl[] = 
{
	#include "tuning_tcl_1_1.inc"
};
unsigned int charge6_tcl_size = sizeof(charge6_tcl);

const __aligned(4) unsigned char charge6_tcl_PB[] = 
{
	#include "tuning_S08_T26_TCL_1_1_DV1_20240919_v2_PTB.inc"
};
unsigned int charge6_tcl_PB_size = sizeof(charge6_tcl_PB);