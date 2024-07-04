/*
* ATS3615 
* Sat 02-10-2023  15:09:23
*/
#ifndef __ATS3615_REG_DEFINITION_H___
#define __ATS3615_REG_DEFINITION_H___

//--------------I2CSLV-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ATS3615_I2CSLV_BASE                                                       0x50000000
#define     ATS3615_I2CSLV_SEED                                                       (ATS3615_I2CSLV_BASE+0x00)
#define     ATS3615_I2CSLV_CRC                                                        (ATS3615_I2CSLV_BASE+0x04)
#define     ATS3615_I2CSLV_ADDR                                                       (ATS3615_I2CSLV_BASE+0x08)

#define     ATS3615_I2CSLV_ADDR_FSEL                                                  16
#define     ATS3615_I2CSLV_ADDR_SDAD_E                                                7
#define     ATS3615_I2CSLV_ADDR_SDAD_SHIFT                                            1
#define     ATS3615_I2CSLV_ADDR_SDAD_MASK                                             (0x7F<<1)

//--------------SPISLV-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ATS3615_SPISLV_BASE                                                       0x50000000
#define     ATS3615_SPISLV_SEED                                                       (ATS3615_SPISLV_BASE+0x0000)
#define     ATS3615_SPISLV_CRC                                                        (ATS3615_SPISLV_BASE+0x0004)

//--------------MEMCTL-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ATS3615_MEMCTL_BASE                                                       0x50008500
#define     ATS3615_MEM_SW                                                            (ATS3615_MEMCTL_BASE+0x08)

#define     ATS3615_MEM_SW_RAM3_SW                                                    3
#define     ATS3615_MEM_SW_RAM2_SW                                                    2
#define     ATS3615_MEM_SW_RAM1_SW                                                    1
#define     ATS3615_MEM_SW_RAM0_SW                                                    0

//--------------CMUD-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ATS3615_CMUD_BASE                                                         0x50008000
#define     ATS3615_CMU_DSPCLK                                                        (ATS3615_CMUD_BASE+0x0000)
#define     ATS3615_CMU_DEVCLKEN                                                      (ATS3615_CMUD_BASE+0x0004)
#define     ATS3615_CMU_BUSCLK                                                        (ATS3615_CMUD_BASE+0x0008)
#define     ATS3615_CMU_SLAVE_CTRL                                                    (ATS3615_CMUD_BASE+0x0300)

//--------------Bits Location------------------------------------------//
#define     ATS3615_CMU_DSPCLK_DSPCLKDIV_E                                            7
#define     ATS3615_CMU_DSPCLK_DSPCLKDIV_SHIFT                                        4
#define     ATS3615_CMU_DSPCLK_DSPCLKDIV_MASK                                         (0xF<<4)
#define     ATS3615_CMU_DSPCLK_DSPCLKSRC_E                                            1
#define     ATS3615_CMU_DSPCLK_DSPCLKSRC_SHIFT                                        0
#define     ATS3615_CMU_DSPCLK_DSPCLKSRC_MASK                                         (0x3<<0)

#define     ATS3615_CMU_BUSCLK_AHBCLKDIV_E                                            1
#define     ATS3615_CMU_BUSCLK_AHBCLKDIV_SHIFT                                        0
#define     ATS3615_CMU_BUSCLK_AHBCLKDIV_MASK                                         (0x3<<0)

//--------------RMU-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ATS3615_RMU_BASE                                                          0x50008400
#define     ATS3615_MRCR0                                                             (ATS3615_RMU_BASE+0x000)
#define     ATS3615_CHIP_VERSION                                                      (ATS3615_RMU_BASE+0x004)
#define     ATS3615_DSP_VCT_ADDR                                                      (ATS3615_RMU_BASE+0x008)

//---------------------------------------------------------------------//

#define ATS3615_WD_BASE     0x50008600
#define ATS3615_WD_CTL      (ATS3615_WD_BASE+0x00)

#define ATS3615_MRCR0_DSP_RESET     0

#endif
