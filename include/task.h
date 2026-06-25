/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Preemptive Multitasking & Scheduler Header (AetherOS-64)
   ============================================================== */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_THREADS 4

/* Thread states */
#define THREAD_STATE_READY     0
#define THREAD_STATE_RUNNING   1
#define THREAD_STATE_SUSPENDED 2

/* Task Control Block (TCB) */
typedef struct thread {
    int id;
    uintptr_t rsp;          // Current stack pointer of the thread
    int state;              // State: READY, RUNNING, or SUSPENDED
} thread_t;

/* Public API Functions */
void task_init(void);
int task_create(void (*entry)(void));
uintptr_t schedule_next(uintptr_t current_rsp);
void task_suspend(int thread_id);
void task_resume(int thread_id);
int task_is_suspended(int thread_id);

#endif /* TASK_H */
