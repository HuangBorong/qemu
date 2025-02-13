#ifndef HW_SECURE_TIMER_H
#define HW_SECURE_TIMER_H

#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_SECURE_TIMER "secure-timer"

typedef struct SecureTimerState SecureTimerState;
DECLARE_INSTANCE_CHECKER(SecureTimerState, SECURE_TIMER, TYPE_SECURE_TIMER)

struct SecureTimerState {
    /* <private> */
    SysBusDevice parent_obj;
    qemu_irq irq;
    
    /* <public> */
    MemoryRegion mmio;
    QEMUTimer timer;

    uint32_t timer_cfg;
    uint32_t timer_compare;
    uint32_t freq_hz;
};

DeviceState* secure_timer_create(MemoryRegion *address_space,
    hwaddr base, qemu_irq irq, uint32_t freq_hz);

#endif /* HW_IBEX_TIMER_H */
