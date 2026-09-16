#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "../include/types.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

void interrupts_init(void);
void pit_init(void);
uint32_t *irq0_handler(uint32_t *current_esp);

#endif
