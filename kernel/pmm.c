#include "pmm.h"

#define PMM_MAX_FRAMES 8192
#define PMM_BITMAP_SIZE (PMM_MAX_FRAMES / 8)

#define E820_ADDR 0x5000
#define E820_COUNT_ADDR 0x4FF0
#define E820_MAX_ENTRIES 32

typedef struct __attribute__((packed))
{
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
    uint32_t acpi;
} e820_entry_t;

static uint8_t frame_bitmap[PMM_BITMAP_SIZE];

static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

extern uint8_t kernel_end;


/* Set a frame as used */
static void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame / 8] |=
        (uint8_t)(1u << (frame % 8));
}


/* Set a frame as free */
static void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame / 8] &=
        (uint8_t)~(1u << (frame % 8));
}


/* Check whether a frame is used */
static int bitmap_test(uint32_t frame)
{
    return (frame_bitmap[frame / 8] &
            (uint8_t)(1u << (frame % 8))) != 0;
}


/* Mark an E820 usable memory region as free */
static void mark_region_free(uint32_t base, uint32_t length)
{
    uint32_t start;
    uint32_t end;
    uint32_t frame;

    if (length == 0)
        return;

    /* Align start upward to 4 KB */
    start = (base + PMM_PAGE_SIZE - 1) &
            ~(PMM_PAGE_SIZE - 1);

    /* Align end downward to 4 KB */
    end = base + length;
    end &= ~(PMM_PAGE_SIZE - 1);

    for (frame = start / PMM_PAGE_SIZE;
         frame < end / PMM_PAGE_SIZE;
         frame++)
    {
        if (frame >= PMM_MAX_FRAMES)
            break;

        /* Never use first 1 MB */
        if (frame < 256)
            continue;

        if (bitmap_test(frame))
        {
            bitmap_clear(frame);
            used_frames--;
        }
    }
}


void pmm_init(void)
{
    uint32_t i;
    uint16_t count;
    e820_entry_t *entries;

    /*
     * Initially mark every frame as USED.
     */
    for (i = 0; i < PMM_BITMAP_SIZE; i++)
        frame_bitmap[i] = 0xFF;

    total_frames = PMM_MAX_FRAMES;
    used_frames = PMM_MAX_FRAMES;


    /*
     * Read E820 memory map created by bootloader.
     */
    count = *(volatile uint16_t *)E820_COUNT_ADDR;

    if (count > E820_MAX_ENTRIES)
        count = E820_MAX_ENTRIES;

    entries = (e820_entry_t *)E820_ADDR;


    /*
     * Only E820 type 1 means usable RAM.
     */
    for (i = 0; i < count; i++)
    {
        if (entries[i].type != 1)
            continue;

        /*
         * Ignore memory above 4 GB.
         * This kernel is 32-bit.
         */
        if (entries[i].base_high != 0 ||
            entries[i].length_high != 0)
            continue;

        mark_region_free(entries[i].base_low,
                         entries[i].length_low);
    }


    /*
     * Protect the kernel itself.
     *
     * Kernel starts at 0x10000.
     * kernel_end comes from linker.ld.
     */
    {
        uint32_t kernel_start = 0x10000;

        uint32_t kernel_finish =
            ((uint32_t)&kernel_end + PMM_PAGE_SIZE - 1)
            & ~(PMM_PAGE_SIZE - 1);

        uint32_t start_frame =
            kernel_start / PMM_PAGE_SIZE;

        uint32_t end_frame =
            kernel_finish / PMM_PAGE_SIZE;

        for (i = start_frame; i < end_frame; i++)
        {
            if (i < PMM_MAX_FRAMES &&
                !bitmap_test(i))
            {
                bitmap_set(i);
                used_frames++;
            }
        }
    }
}


/* Allocate one 4 KB physical frame */
uint32_t pmm_alloc_frame(void)
{
    uint32_t frame;

    for (frame = 0; frame < PMM_MAX_FRAMES; frame++)
    {
        if (!bitmap_test(frame))
        {
            bitmap_set(frame);
            used_frames++;

            return frame * PMM_PAGE_SIZE;
        }
    }

    return 0;
}


/* Free one 4 KB physical frame */
void pmm_free_frame(uint32_t paddr)
{
    uint32_t frame = paddr / PMM_PAGE_SIZE;

    if (frame >= PMM_MAX_FRAMES)
        return;

    /*
     * Never free memory below 1 MB.
     */
    if (frame < 256)
        return;

    if (bitmap_test(frame))
    {
        bitmap_clear(frame);
        used_frames--;
    }
}


uint32_t pmm_get_total_frames(void)
{
    return total_frames;
}


uint32_t pmm_get_used_frames(void)
{
    return used_frames;
}


uint32_t pmm_get_free_frames(void)
{
    return total_frames - used_frames;
}
