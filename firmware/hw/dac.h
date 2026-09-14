#ifndef U4RK_DAC_H
#define U4RK_DAC_H

#include <stdbool.h>
#include <stdint.h>

void u4rk_dac_init(void);
bool u4rk_dac_write(uint16_t value);
uint16_t u4rk_dac_last_value(void);

#endif
