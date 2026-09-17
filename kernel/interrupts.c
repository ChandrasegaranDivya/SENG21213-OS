#include "interrupts.h"
#include "scheduler.h"
#include "thread.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIT_COMMAND  0x43
#define PIT_CHANNEL0 0x40

extern void irq0_stub(void);

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static idt_entry_t idt[256];

static void idt_set_gate(
    int vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t type_attr
)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
}

static void idt_load(void)
{
    idt_ptr_t idt_ptr;

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt[0];

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}

static void pic_remap(void)
{
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* Enable IRQ0 only */
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);

    (void)master_mask;
    (void)slave_mask;
}

void pit_init(void)
{
    uint32_t divisor = 1193180 / 100;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t *irq0_handler(uint32_t *current_esp)
{
    uint32_t *next_esp = scheduler_tick(current_esp);

    outb(PIC1_COMMAND, 0x20);
    return next_esp;
}

void interrupts_init(void)
{
    pic_remap();

    idt_set_gate(
        32,
        (uint32_t)irq0_stub,
        0x08,
        0x8E
    );

    pit_init();
    idt_load();
}
