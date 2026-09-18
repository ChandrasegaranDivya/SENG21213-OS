/* * SENG21213-OS :: Main Kernel  (Stage 0 - Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  - Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  - Threads             →  thread.h  / thread.c
 *   Lecture 11  - Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  - File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc - use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "../include/types.h"
#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupts.h"
#include "pmm.h"
#include "fs.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_memtest(void);
static void cmd_run(void);
static void process_a(void);
static void process_b(void);
static void thread_test(void *arg);
static volatile int myglobal = 0;
static mutex_t race_mutex;
static volatile int race_done = 0;
static volatile int race_finished[2] = {0, 0};
static volatile int mutex_finished[2] = {0, 0};
static volatile int race_phase_done = 0;
static volatile int mutex_result_printed = 0;
static void race_worker(void *arg);
static void race_mutex_worker(void *arg);
static semaphore_t empty_slots;
static semaphore_t full_slots;
static semaphore_t buffer_mutex;

#define PC_BUFFER_SIZE 5

static int pc_buffer[PC_BUFFER_SIZE];
static int pc_in = 0;
static int pc_out = 0;

static void producer(void *arg);
static void consumer(void *arg);
extern void start_first_process(uint32_t *stack_pointer);
/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath - only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  - PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      - kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   - physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         - RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  meminfo  - Show total / used / free physical memory\n");
    vga_puts("  memtest  - Test 100-frame allocation and free\n");
    vga_puts("\n  Process / Thread / Memory:\n");
    vga_puts("  ps      - List processes\n");
    vga_puts("  kill    - Terminate a process\n");
    vga_puts("  threads - List kernel threads\n");
    vga_puts("  free    - Show free memory\n");
    vga_puts("\n  File System (L12):\n");
    vga_puts("  ls      - List files\n");
    vga_puts("  touch   - Create an empty file\n");
    vga_puts("  cat     - Print file contents\n");
    vga_puts("  write   - Append text to a file\n");
    vga_puts("  rm      - Delete a file\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}


static void cmd_mem(void)
{
    uint32_t total = pmm_get_total_frames();
    uint32_t used = pmm_get_used_frames();
    uint32_t free = pmm_get_free_frames();

    uint32_t total_mb =
        (total * PMM_PAGE_SIZE) / (1024 * 1024);

    uint32_t used_mb =
        (used * PMM_PAGE_SIZE) / (1024 * 1024);

    uint32_t free_mb =
        (free * PMM_PAGE_SIZE) / (1024 * 1024);

    vga_puts_color("\n  Physical Memory Manager\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  --------------------------------\n");

    vga_printf("  Total Memory : %u MB\n", total_mb);
    vga_printf("  Used Memory  : %u MB\n", used_mb);
    vga_printf("  Free Memory  : %u MB\n", free_mb);

    vga_printf("  Total Frames : %u\n", total);
    vga_printf("  Used Frames  : %u\n", used);
    vga_printf("  Free Frames  : %u\n", free);

    vga_puts("\n");
}
static void cmd_memtest(void)
{
    uint32_t before = pmm_get_free_frames();
    uint32_t frames[100];
    int i;

    for (i = 0; i < 100; i++)
        frames[i] = pmm_alloc_frame();

    uint32_t after_alloc = pmm_get_free_frames();

    for (i = 0; i < 100; i++)
        pmm_free_frame(frames[i]);

    uint32_t after_free = pmm_get_free_frames();

    vga_puts_color("\n  PMM 100-Frame Test\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_printf("  Before      : %u frames\n", before);
    vga_printf("  After alloc : %u frames\n", after_alloc);
    vga_printf("  After free  : %u frames\n", after_free);

    if (before == after_free)
        vga_puts_color("  RESULT: PASS - No memory leak\n", VGA_LIGHT_GREEN, VGA_BLACK);
    else
        vga_puts_color("  RESULT: FAIL - Memory leak detected\n", VGA_LIGHT_RED, VGA_BLACK);

    vga_puts("\n");
}

static void cmd_ps(void)
{
    vga_puts_color("\n  PID   STATE\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = get_process_by_index(i);

        if (p != 0 && p->state != PROCESS_UNUSED) {
            vga_printf("  %d     ", p->pid);

            if (p->state == PROCESS_READY) {
                vga_puts("READY\n");
            } else if (p->state == PROCESS_RUNNING) {
                vga_puts("RUNNING\n");
            } else if (p->state == PROCESS_TERMINATED) {
                vga_puts("TERMINATED\n");
            }
        }
    }
}

static void cmd_run(void)
{

    int pid_a = create_process(process_a);
    int pid_b = create_process(process_b);

    if (pid_a < 0 || pid_b < 0) {
        vga_puts_color("  Failed to create processes.\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_printf("  Created process A: PID %d\n", pid_a);
    vga_printf("  Created process B: PID %d\n", pid_b);

    int tid = thread_create(thread_test, 0, pid_a);
    vga_printf("  Created thread: TID %d\n", tid);

    myglobal = 0;
    mutex_init(&race_mutex);

    sem_init(&empty_slots, PC_BUFFER_SIZE);
    sem_init(&full_slots, 0);
    sem_init(&buffer_mutex, 1);

    pc_in = 0;
    pc_out = 0;

    int producer_tid = thread_create(producer, 0, pid_a);
    int consumer_tid = thread_create(consumer, 0, pid_a);

    vga_printf("  Producer thread: TID %d\n", producer_tid);
    vga_printf("  Consumer thread: TID %d\n", consumer_tid);

    int race_tid1 = thread_create(race_worker, (void *)0, pid_a);
    int race_tid2 = thread_create(race_worker, (void *)1, pid_a);

    int mutex_tid1 = thread_create(race_mutex_worker, (void *)0, pid_a);
    int mutex_tid2 = thread_create(race_mutex_worker, (void *)1, pid_a);

    vga_printf("  Race threads: TID %d, TID %d\n",
               race_tid1, race_tid2);

    vga_printf("  Mutex race threads: TID %d, TID %d\n",
               mutex_tid1, mutex_tid2);

    cmd_ps();

    __asm__ __volatile__("sti");
}


static void cmd_cat(const char *name)
{
    char buffer[512];
    uint32_t i;
    int fd;
    int bytes_read;

    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: cat <filename>\n");
        return;
    }

    if (!fs_exists(name)) {
        vga_puts("  File not found.\n");
        return;
    }

    for (i = 0; i < sizeof(buffer); i++) {
        buffer[i] = '\0';
    }

    fd = fs_open(name);

    if (fd < 0) {
        vga_puts("  Failed to open file.\n");
        return;
    }

    bytes_read = fs_read(fd, buffer, sizeof(buffer) - 1, 0);

    fs_close(fd);

    if (bytes_read < 0) {
        vga_puts("  Failed to read file.\n");
        return;
    }

    buffer[bytes_read] = '\0';

    vga_puts("  ");
    vga_puts(buffer);
    vga_puts("\n");
}


static void cmd_delete(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: delete <filename>\n");
        return;
    }

    if (fs_unlink(name)) {
        vga_puts("  File deleted.\n");
    } else {
        vga_puts("  File not found.\n");
    }
}

static void cmd_create(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: create <filename>\n");
        return;
    }

    if (fs_create(name)) {
        vga_puts("  File created.\n");
    } else {
        vga_puts("  Failed to create file.\n");
    }
}


static void cmd_write(const char *args)
{
    char name[FS_MAX_FILENAME];
    const char *data;
    uint32_t i;
    uint32_t len;
    int fd;
    int bytes_written;

    if (args == 0 || args[0] == '\0') {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    i = 0;

    while (args[i] != '\0' && args[i] != ' ' && i < FS_MAX_FILENAME - 1) {
        name[i] = args[i];
        i++;
    }

    name[i] = '\0';

    while (args[i] == ' ') {
        i++;
    }

    data = args + i;

    if (name[0] == '\0' || data[0] == '\0') {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    len = k_strlen(data);

    if (len > FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) {
        vga_puts("  Text too long.\n");
        return;
    }

    if (!fs_exists(name)) {
        vga_puts("  File not found.\n");
        return;
    }

    fd = fs_open(name);

    if (fd < 0) {
        vga_puts("  Failed to open file.\n");
        return;
    }

    bytes_written = fs_write(fd, data, len, fs_size(fd));

    fs_close(fd);

    if (bytes_written == (int)len) {
        vga_puts("  File written.\n");
    } else {
        vga_puts("  Failed to write file.\n");
    }
}

static void process_a(void)
{
    while (true) {
        vga_puts_color("A", VGA_LIGHT_GREEN, VGA_BLACK);

        for (volatile uint32_t i = 0; i < 1000; i++) {
        }
    }
}

static void process_b(void)
{
    while (true) {
        vga_puts_color("B", VGA_LIGHT_CYAN, VGA_BLACK);

        for (volatile uint32_t i = 0; i < 1000000; i++) {
        }
    }
}


static void thread_test(void *arg)
{
    (void)arg;

    while (true) {
        vga_puts_color("T", VGA_YELLOW, VGA_BLACK);

        for (volatile uint32_t i = 0; i < 500000; i++) {
        }
    }
}
static void producer(void *arg)
{
    (void)arg;

    for (int item = 1; item <= 10; item++) {

        sem_wait(&empty_slots);
        sem_wait(&buffer_mutex);

        pc_buffer[pc_in] = item;
        pc_in = (pc_in + 1) % PC_BUFFER_SIZE;

        vga_printf("\nProducer: %d", item);

        sem_signal(&buffer_mutex);
        sem_signal(&full_slots);
    }
}

static void consumer(void *arg)
{
    (void)arg;

    for (int i = 0; i < 10; i++) {

        sem_wait(&full_slots);
        sem_wait(&buffer_mutex);

        int item = pc_buffer[pc_out];
        pc_out = (pc_out + 1) % PC_BUFFER_SIZE;

        vga_printf("\nConsumer: %d", item);

        sem_signal(&buffer_mutex);
        sem_signal(&empty_slots);
    }
}
/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
if (k_strcmp(cmd, "mem") == 0) { cmd_mem(); continue; }
        if (k_strcmp(cmd, "meminfo") == 0) { cmd_mem(); continue; }
        if (k_strcmp(cmd, "memtest") == 0) { cmd_memtest(); continue; }
        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strncmp(cmd, "cat ", 4) == 0) {
            cmd_cat(k_ltrim(cmd + 4));
            continue;
        }

        if (k_strcmp(cmd, "ls") == 0) {
            fs_list();
            continue;
        }

        if (k_strncmp(cmd, "create ", 7) == 0) {
            cmd_create(k_ltrim(cmd + 7));
            continue;
        }

        if (k_strncmp(cmd, "touch ", 6) == 0) {
            cmd_create(k_ltrim(cmd + 6));
            continue;
        }

        if (k_strncmp(cmd, "write ", 6) == 0) {
            cmd_write(k_ltrim(cmd + 6));
            continue;
        }

        if (k_strncmp(cmd, "delete ", 7) == 0) {
            cmd_delete(k_ltrim(cmd + 7));
            continue;
        }


        if (k_strncmp(cmd, "rm ", 3) == 0) {
            cmd_delete(k_ltrim(cmd + 3));
            continue;
        }


        /* Milestone stubs */
/* Stage 1: process listing */
if (k_strcmp(cmd, "run") == 0) {
    cmd_run();
    continue;
}

if (k_strcmp(cmd, "ps") == 0) {
    cmd_ps();
    continue;
}
if (k_strcmp(cmd, "threads") == 0) {
    vga_puts("TID  STATE       PID\n");

    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = get_thread_by_index(i);

        if (t != 0 && t->state != THREAD_UNUSED) {
            vga_puts("Thread found\n");
        }
    }

    continue;
}


/* Milestone stubs */
if (k_strcmp(cmd, "kill")    == 0 ||
    k_strcmp(cmd, "threads") == 0 ||
    k_strcmp(cmd, "free")    == 0 ||
    k_strcmp(cmd, "ls")      == 0 ||
    k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point - called from kernel_entry.asm
 * --------------------------------------------------------------------------*/

void kernel_main(void) {
    vga_init();
    vga_puts("TEST 1\n");

    kb_init();
    vga_puts("TEST 2\n");

    process_init();
    vga_puts("TEST 3\n");

    thread_init();
    vga_puts("TEST 4\n");

    // scheduler_init();
    pmm_init();
    fs_init();
    // interrupts_init();
    // __asm__ __volatile__("sti");

    vga_puts("TEST 5\n");

    print_splash();
    shell_run();

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}
static void race_worker(void *arg)
{
    int id = (int)(uint32_t)arg;

    for (int i = 0; i < 10000; i++) {
        int temp = myglobal;

        for (volatile int j = 0; j < 1000; j++) {
        }

        myglobal = temp + 1;
    }

    race_finished[id] = 1;

    if (race_finished[0] && race_finished[1] && !race_phase_done) {
        race_phase_done = 1;

        vga_printf("\nRace WITHOUT mutex: %d (expected 20000)\n",
                   myglobal);

        myglobal = 0;
        mutex_init(&race_mutex);
    }
}

static void race_mutex_worker(void *arg)
{
    int id = (int)(uint32_t)arg;

    while (!race_phase_done) {
        __asm__ __volatile__("hlt");
    }

    for (int i = 0; i < 10000; i++) {
        mutex_lock(&race_mutex);

        int temp = myglobal;

        for (volatile int j = 0; j < 1000; j++) {
        }

        myglobal = temp + 1;

        mutex_unlock(&race_mutex);
    }

    mutex_finished[id] = 1;

    if (mutex_finished[0] && mutex_finished[1] &&
        !mutex_result_printed) {
        mutex_result_printed = 1;

        vga_printf("\nRace WITH mutex: %d (expected 20000)\n",
                   myglobal);
    }
}
