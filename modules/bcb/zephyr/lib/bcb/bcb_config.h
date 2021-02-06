#ifndef _BCB_CONFIG_H_
#define _BCB_CONFIG_H_

#include <stdint.h>
#include <sys/types.h>

int bcb_config_init(void);
int bcb_config_load(off_t offset, uint8_t *data, size_t size);
int bcb_config_store(off_t offset, uint8_t *data, size_t size);

#endif // _BCB_CONFIG_H_