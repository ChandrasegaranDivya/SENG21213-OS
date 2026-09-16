BITS 32

global switch_context
global start_first_process
global irq0_stub

extern irq0_handler

switch_context:
    pushad
    popad
    ret

start_first_process:
    mov esp, [esp + 4]
    popad
    iretd

irq0_stub:
    pusha
    push esp
    call irq0_handler
    add esp, 4
    mov esp, eax
    popa
    iretd
