/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Preemptive Multitasking & Round-Robin Scheduler (AetherOS-64)
   ============================================================== */

#include "task.h"
#include "pmm.h"
#include "pit.h"

static thread_t threads[MAX_THREADS];
static int num_threads = 0;
static int current_thread_index = 0;

/* task_init: Establishes the main execution context as Thread 0 */
void task_init(void) {
    threads[0].id = 0;
    threads[0].rsp = 0; // Will be populated dynamically on the first timer tick
    threads[0].state = THREAD_STATE_RUNNING;
    num_threads = 1;
    current_thread_index = 0;
}

/* task_create: Allocates a stack page, forces stack frame structure, and registers thread */
int task_create(void (*entry)(void)) {
    if (num_threads >= MAX_THREADS) {
        return -1;
    }

    // Allocate 4KB physical stack page
    void* stack_base = pmm_alloc_block();
    if (!stack_base) {
        return -1;
    }

    // Set stack pointer to the top of the page (stacks grow downwards)
    uint64_t* stack = (uint64_t*)((uintptr_t)stack_base + 4096);

    // 1. Forge the CPU interrupt frame expected by iretq:
    // Highest addresses popped last
    *(--stack) = 0x10;                          // SS (Data segment selector)
    *(--stack) = (uintptr_t)stack_base + 4096;  // RSP
    *(--stack) = 0x202;                         // RFLAGS (bit 9 set enables interrupts)
    *(--stack) = 0x08;                          // CS (Code segment selector)
    *(--stack) = (uintptr_t)entry;              // RIP (Entry point function)

    // 2. Forge general-purpose GPR registers popped by assembly wrapper:
    // Pushed RAX (highest address) to R15 (lowest address)
    *(--stack) = 0; // RAX
    *(--stack) = 0; // RBX
    *(--stack) = 0; // RCX
    *(--stack) = 0; // RDX
    *(--stack) = 0; // RSI
    *(--stack) = 0; // RDI
    *(--stack) = 0; // RBP
    *(--stack) = 0; // R8
    *(--stack) = 0; // R9
    *(--stack) = 0; // R10
    *(--stack) = 0; // R11
    *(--stack) = 0; // R12
    *(--stack) = 0; // R13
    *(--stack) = 0; // R14
    *(--stack) = 0; // R15 (First GPR popped by isr)

    // Register the TCB entry
    int id = num_threads;
    threads[id].id = id;
    threads[id].rsp = (uintptr_t)stack; // Stack pointer points to bottom GPR (R15)
    threads[id].state = THREAD_STATE_READY;
    num_threads++;

    return id;
}

/* schedule_next: Saves the stack pointer of current task and switches to the next ready task */
uintptr_t schedule_next(uintptr_t current_rsp) {
    // 1. Increment timer tick counter
    pit_increment_ticks();

    // 2. Save stack pointer of currently running thread
    threads[current_thread_index].rsp = current_rsp;
    if (threads[current_thread_index].state != THREAD_STATE_SUSPENDED) {
        threads[current_thread_index].state = THREAD_STATE_READY;
    }

    // 3. Cycle to the next registered thread in a round-robin circle, skipping suspended ones
    int attempts = 0;
    do {
        current_thread_index = (current_thread_index + 1) % num_threads;
        attempts++;
    } while (threads[current_thread_index].state == THREAD_STATE_SUSPENDED && attempts < num_threads);

    // 4. Mark selected thread as running and return its stack pointer
    threads[current_thread_index].state = THREAD_STATE_RUNNING;
    return threads[current_thread_index].rsp;
}

void task_suspend(int thread_id) {
    if (thread_id >= 0 && thread_id < num_threads) {
        threads[thread_id].state = THREAD_STATE_SUSPENDED;
    }
}

void task_resume(int thread_id) {
    if (thread_id >= 0 && thread_id < num_threads) {
        threads[thread_id].state = THREAD_STATE_READY;
    }
}

int task_is_suspended(int thread_id) {
    if (thread_id >= 0 && thread_id < num_threads) {
        return threads[thread_id].state == THREAD_STATE_SUSPENDED;
    }
    return 0;
}
