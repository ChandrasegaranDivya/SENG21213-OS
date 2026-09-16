; ============================================================================
; SENG21213-OS :: Kernel Entry Point
; File   : kernel/kernel_entry.asm
; Purpose: Bridges the bootloader to the C kernel.
; ============================================================================

[BITS 32]

[EXTERN kernel_main]
[GLOBAL _start]

_start:
    ; Disable interrupts before entering the kernel.
    cli

    ; ------------------------------------------------------------
    ; Direct VGA test
    ; If this appears, kernel entry point is being executed.
    ; ------------------------------------------------------------

    mov edi, 0xB8000

    mov word [edi],      0x0F45    ; E
    mov word [edi + 2],  0x0F4E    ; N
    mov word [edi + 4],  0x0F54    ; T
    mov word [edi + 6],  0x0F52    ; R
    mov word [edi + 8],  0x0F59    ; Y

    ; ------------------------------------------------------------
    ; Call the C kernel main function.
    ; ------------------------------------------------------------

    call kernel_main

    ; If kernel_main ever returns, halt the CPU permanently.
.halt:
    hlt
    jmp .halt
