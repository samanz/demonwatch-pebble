#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
typedef void *ResHandle;
#define RESOURCE_ID_E1M1_WAD 1
ResHandle resource_get_handle(uint32_t);
size_t resource_size(ResHandle);
size_t resource_load_byte_range(ResHandle,uint32_t,uint8_t *,size_t);
void app_log(uint8_t,const char *,int,const char *,...);
