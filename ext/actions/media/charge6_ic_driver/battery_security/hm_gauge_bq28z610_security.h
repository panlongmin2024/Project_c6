#ifndef _HM_GAUGE_BQ28Z610_SECURITY_H_
#define _HM_GAUGE_BQ28Z610_SECURITY_H_




#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char se_uint8;
typedef unsigned char se_uint16;
typedef unsigned int se_uint32;
typedef unsigned char se_bool;

unsigned char is_authenticated(void);
se_bool is_backup_authenticated(se_uint8* sn, se_uint8* rsn);


#endif
