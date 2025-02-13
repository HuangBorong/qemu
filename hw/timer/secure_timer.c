#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "system/system.h"
#include "system/tcg.h"
#include "migration/vmstate.h"
#include "memory.h"
#include "hw/timer/secure_timer.h"

#define A_TIMER_CFG          0x00
#define A_TIMER_COMPARE      0x04

static uint64_t cpu_riscv_read_rtc(uint32_t timebase_freq)
{
    return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                    timebase_freq, NANOSECONDS_PER_SECOND);
}

static void secure_timer_update_irqs(SecureTimerState *s)
{
    uint64_t value = s->timer_compare;
    uint64_t now = cpu_riscv_read_rtc(s->freq_hz);
    uint64_t diff, next;

    if (s->timer_cfg == 0) 
    {
        return;
    }
    
    if (now >= value) 
    {
        info_report("secure_timer: interrupt\n");
        qemu_irq_raise(s->irq);
    }
    else
    {
        info_report("secure_timer: no interrupt\n");
        qemu_irq_lower(s->irq);
        diff = now - value;
        next = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                                 muldiv64(diff,
                                          NANOSECONDS_PER_SECOND,
                                          s->freq_hz);
        timer_mod(&s->timer, next);
    } 
}
static uint64_t secure_timer_read(void *opaque, hwaddr addr, unsigned int size)
{
    SecureTimerState* s = opaque;
    uint32_t val = 0;

    switch (addr) 
    {
    case A_TIMER_CFG:
        val = s->timer_cfg;
        info_report("read timer_cfg, val: %u\n",val);
        break;
    case A_TIMER_COMPARE:
        val = s->timer_compare;
        info_report("read timer_compare, val: %u\n",val);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        break;
    }

    return val;
}

static void secure_timer_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    SecureTimerState* s = opaque;
    uint32_t val = val64;

    switch (addr) 
    {
    case A_TIMER_CFG:
        s->timer_cfg = val;
        info_report("write timer_cfg, val: %u\n",val);
        break;
    case A_TIMER_COMPARE:
        s->timer_compare = val;
        info_report("write timer_compare, val: %u\n",val);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        break;
    }

    secure_timer_update_irqs(s);
}

static const MemoryRegionOps secure_timer_ops = {
    .read = secure_timer_read,
    .write = secure_timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static const Property secure_timer_properties[] = {
    DEFINE_PROP_UINT32("clock-frequency", SecureTimerState, freq_hz, 10000),
};

static void secure_timer_cb(void *opaque)
{
    SecureTimerState *s = opaque;

    info_report("%s secure timer interrupt raise\n", __func__);
    qemu_irq_raise(s->irq);
    qemu_irq_lower(s->irq);
}

static void secure_timer_realize(DeviceState *dev, Error **errp)
{
    SecureTimerState *s = SECURE_TIMER(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &secure_timer_ops, s,
                    TYPE_SECURE_TIMER, 0x200);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->irq);
    timer_init_ns(&s->timer, QEMU_CLOCK_VIRTUAL,
        secure_timer_cb, s);
}

static const VMStateDescription vmstate_secure_timer = {
    .name = TYPE_SECURE_TIMER,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(timer_cfg, SecureTimerState),
        VMSTATE_UINT32(timer_compare, SecureTimerState),
        VMSTATE_END_OF_LIST()
    }
};

static void secure_timer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, secure_timer_properties);
    dc->realize = secure_timer_realize;
    dc->vmsd = &vmstate_secure_timer;
}

static const TypeInfo secure_timer_info = {
    .name          = TYPE_SECURE_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SecureTimerState),
    .class_init    = secure_timer_class_init,
};

static void secure_timer_register_types(void)
{
    type_register_static(&secure_timer_info);
}

type_init(secure_timer_register_types)

DeviceState* secure_timer_create(MemoryRegion *address_space,
    hwaddr base, qemu_irq irq, uint32_t freq_hz)
{
    DeviceState *dev = qdev_new(TYPE_SECURE_TIMER);
    SecureTimerState *s = SECURE_TIMER(dev);
    MemoryRegion *mr;

    qdev_prop_set_uint32(dev, "clock-frequency", freq_hz);
    qdev_set_legacy_instance_id(DEVICE(s), base, 2);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    sysbus_connect_irq(SYS_BUS_DEVICE(s), 0, irq);
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(s), 0);
    memory_region_add_subregion(address_space, base, mr);

    return dev;
}