#ifndef U4RK_ACQUISITION_H
#define U4RK_ACQUISITION_H

#include "u4rk.h"

typedef enum {
    U4RK_CAPTURE_IDLE = 0,
    U4RK_CAPTURE_ACTIVE,
    U4RK_CAPTURE_DONE,
    U4RK_CAPTURE_DMA_FAULT,
} u4rk_capture_state_t;

void u4rk_acquisition_init(void);
bool u4rk_capture_start(uint16_t *destination);
u4rk_capture_state_t u4rk_capture_poll(void);
void u4rk_capture_abort(void);

void u4rk_pulser_arm(void);
void u4rk_pulser_disarm(void);
bool u4rk_pulser_is_armed(void);
bool u4rk_pulser_configure(uint32_t negative_ns, uint32_t damp_ns,
                           uint32_t positive_ns, u4rk_pulse_order_t order);
u4rk_pulse_config_t u4rk_pulser_get_config(void);

#endif
