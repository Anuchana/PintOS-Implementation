# Pintos Threads Project

Implementation and study notes for the Pintos **Threads** project.

This project was developed progressively through the following stages:

1. Alarm Clock / Thread Sleeping
2. Priority Scheduling
3. Priority Donation
4. Nested Priority Donation
5. Priority-Aware Synchronization
6. MLFQS — Multi-Level Feedback Queue Scheduler
7. Fixed-Point Arithmetic
8. Testing and Debugging

---

# Table of Contents

* [1. Project Overview](#1-project-overview)
* [2. Project Structure](#2-project-structure)
* [3. Alarm Clock](#3-alarm-clock)

  * [3.1 Problem](#31-problem)
  * [3.2 Original Approach](#32-original-approach)
  * [3.3 Sleep List](#33-sleep-list)
  * [3.4 `timer_sleep()`](#34-timer_sleep)
  * [3.5 `thread_sleep()`](#35-thread_sleep)
  * [3.6 `timer_interrupt()`](#36-timer_interrupt)
  * [3.7 `thread_unblock()`](#37-thread_unblock)
  * [3.8 Alarm Clock Flow](#38-alarm-clock-flow)
* [4. Priority Scheduling](#4-priority-scheduling)

  * [4.1 Priority Concept](#41-priority-concept)
  * [4.2 Ready List](#42-ready-list)
  * [4.3 `thread_create()`](#43-thread_create)
  * [4.4 `thread_yield()`](#44-thread_yield)
  * [4.5 `thread_set_priority()`](#45-thread_set_priority)
  * [4.6 `thread_get_priority()`](#46-thread_get_priority)
  * [4.7 Preemption](#47-preemption)
* [5. Priority Donation](#5-priority-donation)

  * [5.1 Priority Inversion](#51-priority-inversion)
  * [5.2 Donation Concept](#52-donation-concept)
  * [5.3 Base and Effective Priority](#53-base-and-effective-priority)
  * [5.4 Donation Data Structure](#54-donation-data-structure)
  * [5.5 `lock_acquire()`](#55-lock_acquire)
  * [5.6 `lock_release()`](#56-lock_release)
  * [5.7 Nested Donation](#57-nested-donation)
  * [5.8 Multiple Donations](#58-multiple-donations)
  * [5.9 Priority Restoration](#59-priority-restoration)
* [6. Priority-Aware Synchronization](#6-priority-aware-synchronization)

  * [6.1 Semaphores](#61-semaphores)
  * [6.2 Condition Variables](#62-condition-variables)
  * [6.3 Priority Ordering](#63-priority-ordering)
* [7. MLFQS](#7-mlfqs)

  * [7.1 What is MLFQS?](#71-what-is-mlfqs)
  * [7.2 Why MLFQS?](#72-why-mlfqs)
  * [7.3 Nice Value](#73-nice-value)
  * [7.4 Recent CPU](#74-recent-cpu)
  * [7.5 Load Average](#75-load-average)
  * [7.6 Dynamic Priority](#76-dynamic-priority)
  * [7.7 Required Functions](#77-required-functions)
* [8. Fixed-Point Arithmetic](#8-fixed-point-arithmetic)

  * [8.1 Why Fixed Point?](#81-why-fixed-point)
  * [8.2 Representation](#82-representation)
  * [8.3 Conversion](#83-conversion)
  * [8.4 Arithmetic](#84-arithmetic)
* [9. Important Pintos Functions](#9-important-pintos-functions)
* [10. Files Modified / Related](#10-files-modified--related)
* [11. Testing](#11-testing)
* [12. Debugging History](#12-debugging-history)
* [13. Common Problems](#13-common-problems)
* [14. Final Conceptual Flow](#14-final-conceptual-flow)

---

# 1. Project Overview

The Pintos Threads project focuses on understanding how an operating system manages:

* threads
* scheduling
* synchronization
* sleeping and waking
* priorities
* priority inversion
* priority donation
* CPU scheduling
* dynamic priorities
* CPU usage accounting

The implementation was approached incrementally.

```text
                    Pintos Threads
                         |
          +--------------+--------------+
          |              |              |
      Alarm Clock    Priority       Synchronization
          |          Scheduling           |
          |              |                |
     Sleep/Wake     Priority List      Locks/Semaphores
                         |
                  Priority Donation
                         |
                    Nested Donation
                         |
                       MLFQS
                         |
              +----------+----------+
              |          |          |
            nice     recent_cpu   load_avg
              \          |          /
               \         |         /
                Dynamic Priority
```

---

# 2. Project Structure

The main work is inside:

```text
src/
└── threads/
    ├── init.c
    ├── thread.c
    ├── thread.h
    ├── synch.c
    ├── synch.h
    ├── timer.c
    ├── timer.h
    └── Makefile
```

The test suite is under:

```text
src/tests/threads/
```

Important test groups include:

```text
Alarm Clock:
    alarm-single
    alarm-multiple
    alarm-simultaneous
    alarm-priority
    alarm-zero
    alarm-negative

Priority:
    priority-change
    priority-fifo
    priority-preempt
    priority-sema
    priority-condvar

Priority Donation:
    priority-donate-one
    priority-donate-multiple
    priority-donate-multiple2
    priority-donate-nest
    priority-donate-sema
    priority-donate-lower
    priority-donate-chain

MLFQS:
    mlfqs-load-1
    mlfqs-load-60
    mlfqs-load-avg
    mlfqs-recent-1
    mlfqs-fair-2
    mlfqs-fair-20
    mlfqs-nice-2
    mlfqs-nice-10
    mlfqs-block
```

The saved test logs confirm these test groups were being run together during development.

---

# 3. Alarm Clock

## 3.1 Problem

The original Pintos implementation of:

```c
timer_sleep()
```

typically wastes CPU time by repeatedly checking the current tick.

Conceptually:

```text
while (current_ticks < wake_time)
{
    thread_yield();
}
```

This is inefficient because the thread remains runnable even though it has nothing useful to do.

The correct solution is:

```text
Running
   |
   | timer_sleep()
   v
Blocked
   |
   | wait until wake_tick
   v
Ready
   |
   v
Running
```

The sleeping thread should **not consume CPU time while waiting**.

---

# 3.2 Original Approach

A sleeping thread needs a value indicating when it should wake up.

Therefore a field such as:

```c
int64_t wake_tick;
```

is associated with the thread.

A global sleep list is also required:

```c
struct list sleep_list;
```

Each sleeping thread is inserted into this list.

---

# 3.3 Sleep List

The sleep list contains threads that are currently sleeping.

Example:

```text
sleep_list

+---------+------------+
| Thread  | wake_tick  |
+---------+------------+
| T1      | 100        |
| T2      | 150        |
| T3      | 200        |
+---------+------------+
```

At every timer tick, the kernel checks whether a sleeping thread has reached its wake time.

---

# 3.4 `timer_sleep()`

### Purpose

```c
timer_sleep(int64_t ticks)
```

makes the current thread sleep for a specified number of timer ticks.

The important idea is:

```text
current tick = 500
requested sleep = 100

wake_tick = 500 + 100
          = 600
```

The thread should remain blocked until tick `600`.

### Conceptual implementation

```c
void
timer_sleep(int64_t ticks)
{
    if (ticks <= 0)
        return;

    int64_t wake_tick = timer_ticks() + ticks;

    thread_sleep(wake_tick);
}
```

The exact implementation can vary, but the important separation is:

```text
timer_sleep()
      |
      v
calculate wake time
      |
      v
thread_sleep()
      |
      v
BLOCKED
```

---

# 3.5 `thread_sleep()`

### Purpose

`thread_sleep()` moves the current thread from the running state to the sleeping state.

The implementation developed during the project followed this structure:

```c
void thread_sleep(int64_t ticks)
{
    struct thread *current = thread_current();
    enum intr_level old_level = intr_disable();

    if (current != idle_thread)
    {
        current->wake_tick = ticks;

        list_push_back(&sleep_list, &current->elem);

        current->status = THREAD_BLOCKED;

        schedule();
    }

    intr_set_level(old_level);
}
```

The saved implementation confirms this exact design: interrupts are disabled, `wake_tick` is stored, the thread is inserted into `sleep_list`, its state becomes `THREAD_BLOCKED`, and `schedule()` is called.

### Why disable interrupts?

Because the sleep list is shared between:

```text
Normal kernel code
        +
Timer interrupt
```

Without disabling interrupts, the list could be modified concurrently.

For example:

```text
Thread:
    add itself to sleep_list

Timer interrupt:
    remove thread from sleep_list
```

This could produce a race condition.

Therefore:

```c
intr_disable();
```

protects the critical section.

---

# 3.6 `timer_interrupt()`

The timer interrupt executes once every timer tick.

Conceptually:

```text
Timer interrupt
      |
      v
increase ticks
      |
      v
check sleep_list
      |
      v
wake expired threads
```

For every sleeping thread:

```c
if (thread->wake_tick <= current_tick)
{
    thread_unblock(thread);
}
```

Example:

```text
Current tick = 100

T1 wake_tick = 90   -> wake
T2 wake_tick = 100  -> wake
T3 wake_tick = 120  -> remain sleeping
```

---

# 3.7 `thread_unblock()`

`thread_unblock()` moves a blocked thread back to the ready state.

Conceptually:

```text
BLOCKED
   |
   | thread_unblock()
   v
READY
```

It inserts the thread into the ready list.

With priority scheduling, the ready list should be maintained according to priority.

---

# 3.8 Alarm Clock Flow

Complete flow:

```text
             timer_sleep(100)
                    |
                    v
             calculate wake_tick
                    |
                    v
             thread_sleep()
                    |
             disable interrupts
                    |
             add to sleep_list
                    |
             status = BLOCKED
                    |
                 schedule()
                    |
                    v
               another thread
                    |
                    |
             timer interrupt
                    |
                    v
             current tick++
                    |
                    v
             check sleep_list
                    |
          +---------+---------+
          |                   |
     not expired           expired
          |                   |
          v                   v
       remain              unblock
                              |
                              v
                            READY
```

---

# 4. Priority Scheduling

After the alarm clock, the next major step is priority scheduling.

## 4.1 Priority Concept

Each thread has a priority.

For example:

```text
Thread A = 10
Thread B = 30
Thread C = 20
```

The scheduler should choose:

```text
Thread B
```

because:

```text
30 > 20 > 10
```

Pintos uses larger numeric values for higher priority.

---

# 4.2 Ready List

The ready list contains threads that can run.

Instead of:

```text
T1
T2
T3
T4
```

we maintain it in priority order:

```text
Priority
   ^
   |
  50  T4
  40  T2
  30  T1
  10  T3
```

The first element is therefore the highest-priority thread.

The saved `thread_yield()` implementation inserts the current thread using a priority comparator.

---

# 4.3 `thread_create()`

### Purpose

Creates a new thread.

Important steps:

```text
allocate thread
      |
      v
initialize thread
      |
      v
assign priority
      |
      v
insert into ready list
      |
      v
possibly preempt current thread
```

The implementation also checks whether the newly created thread has a higher priority than the current thread and yields if necessary.

---

# 4.4 `thread_yield()`

### Purpose

The current thread voluntarily gives up the CPU.

Conceptually:

```text
RUNNING
   |
   | thread_yield()
   v
READY
   |
   v
scheduler chooses next thread
```

The current thread is inserted back into the ready list.

With priority scheduling:

```c
list_insert_ordered(&ready_list,
                    &cur->elem,
                    cmp_priority,
                    NULL);
```

The saved implementation follows this priority-ordered ready-list approach.

---

# 4.5 `thread_set_priority()`

### Purpose

Changes the base priority of the current thread.

The important distinction is:

```text
base_priority
       |
       v
priority
```

`base_priority` is the priority chosen by the thread itself.

`priority` is the thread's **effective priority**.

With donation, these can be different.

The implementation developed contains:

```c
thread_current()->base_priority = new_priority;
```

and then recalculates the effective priority based on outstanding donations.

---

# 4.6 `thread_get_priority()`

### Purpose

Returns the current thread's effective priority.

```c
int
thread_get_priority(void)
{
    return thread_current()->priority;
}
```

The saved implementation uses exactly this concept.

---

# 4.7 Preemption

Suppose:

```text
Current thread = priority 20

Ready thread = priority 50
```

The current thread should not continue running.

Instead:

```text
20
 |
 | higher priority thread exists
 v
yield
 |
 v
50 runs
```

This is called **preemption**.

The project also uses `thread_tick()` to enforce time-slice preemption. When the time slice expires, `intr_yield_on_return()` requests a context switch when the interrupt returns.

---

# 5. Priority Donation

Priority scheduling introduces a major problem:

## Priority Inversion

Consider three threads:

```text
H = High priority
M = Medium priority
L = Low priority
```

Suppose:

```text
L owns lock X
H needs lock X
```

Now:

```text
H -> waiting for L
```

But:

```text
M -> ready to run
```

The scheduler may choose:

```text
M
M
M
M
...
```

because:

```text
M > L
```

Therefore `L` cannot run and release the lock.

The high-priority thread is effectively blocked by the low-priority thread.

This is **priority inversion**.

---

# 5.1 Priority Inversion

Without donation:

```text
       High Priority
            H
            |
            | wants lock
            v
       Low Priority
            L
            |
            | owns lock
            v
          LOCK

Meanwhile:

       Medium Priority
              M
              |
              v
          CPU time
```

The medium-priority thread can prevent the low-priority thread from releasing the lock.

---

# 5.2 Donation Concept

The high-priority thread temporarily gives its priority to the low-priority lock holder.

```text
Before:

H = 50
M = 30
L = 10

After donation:

H = 50
M = 30
L = 50   <- donated priority
```

Now the scheduler chooses `L`.

```text
L runs
 |
 | releases lock
 v
H runs
```

This solves priority inversion.

---

# 5.3 Base and Effective Priority

Every thread should conceptually maintain:

```c
int base_priority;
int priority;
```

Example:

```text
base_priority = 20
priority      = 50
```

Why?

Because the thread originally wanted:

```text
20
```

but temporarily received:

```text
50
```

from another thread.

When the donation ends:

```text
priority -> 20
```

The base priority is not permanently changed by donation.

---

# 5.4 Donation Data Structure

The implementation introduced donation records containing information such as:

```text
donor
lock
priority
```

Conceptually:

```c
struct donation
{
    struct list_elem elem;
    struct thread *donor;
    struct lock *lock;
    int priority;
};
```

A thread can therefore have:

```text
donations
   |
   +---- Donation from T1
   |
   +---- Donation from T2
   |
   +---- Donation from T3
```

---

# 5.5 `lock_acquire()`

This is one of the most important functions.

Normal lock acquisition:

```text
lock_acquire(lock)
       |
       v
Is lock free?
   /       \
 yes       no
  |         |
  v         v
 acquire   wait
```

With priority donation:

```text
lock_acquire(lock)
       |
       v
Is lock held?
       |
      yes
       |
       v
Compare priorities
       |
       v
donate if necessary
       |
       v
wait for lock
```

The implementation checks the lock holder and propagates the current thread's priority through the waiting-lock chain.

---

# 5.6 `lock_release()`

When a lock is released, donations associated with that lock must be removed.

Conceptually:

```text
lock_release(X)
       |
       v
remove donations caused by X
       |
       v
recalculate effective priority
       |
       v
release lock
       |
       v
wake waiting thread
```

The saved implementation removes donation records whose associated lock matches the released lock, then recalculates the effective priority.

---

# 5.7 Nested Donation

Nested donation occurs when donation itself needs to travel through another lock.

Example:

```text
H priority = 50
M priority = 30
L priority = 10
```

Suppose:

```text
L owns Lock A

M owns Lock B

M waits for Lock A
```

Then:

```text
H waits for Lock B
```

The dependency chain becomes:

```text
H
 |
 | waits for B
 v
M
 |
 | waits for A
 v
L
```

Therefore:

```text
H priority = 50
        |
        v
M receives 50
        |
        v
L receives 50
```

The implementation follows the `waiting_lock` chain:

```text
current
   |
waiting_lock
   |
holder
   |
waiting_lock
   |
holder
   |
...
```

This is the basis of nested donation.

---

# 5.8 Multiple Donations

A thread can receive donations from several threads.

Example:

```text
Base priority = 10

Donation 1 = 30
Donation 2 = 50
Donation 3 = 40
```

Effective priority should be:

```text
max(10, 30, 50, 40)

= 50
```

Therefore:

```text
effective priority = maximum(base priority,
                              all active donations)
```

The implementation uses a donation comparator and `list_max()` to find the highest donation.

---

# 5.9 Priority Restoration

When one lock is released:

```text
Base = 10

Donation A = 50
Donation B = 30
```

Before release:

```text
priority = 50
```

If Donation A disappears:

```text
priority = max(10, 30)
         = 30
```

If Donation B also disappears:

```text
priority = 10
```

Therefore priority should not simply be reset immediately to the base priority if other donations are still active.

---

# 6. Priority-Aware Synchronization

Priority scheduling must work together with synchronization.

The important synchronization primitives are:

```text
Semaphore
Lock
Condition Variable
```

---

# 6.1 Semaphores

A semaphore contains:

```text
value
waiters
```

A thread executing:

```c
sema_down()
```

may have to wait.

The scheduler should wake the highest-priority waiter first.

Conceptually:

```text
Semaphore waiters:

T1 priority 10
T2 priority 50
T3 priority 30
```

Wake:

```text
T2
```

first.

---

# 6.2 Condition Variables

Condition variables also contain waiting threads.

The important idea is:

```text
condition.waiters
```

must be handled according to priority when priority scheduling is enabled.

The project included priority condition-variable testing as part of the priority suite.

---

# 6.3 Priority Ordering

Priority-aware synchronization therefore has this general rule:

```text
Whenever the kernel chooses one waiting thread:

choose highest priority first.
```

This applies to:

```text
ready list
semaphore waiters
condition variable waiters
```

---

# 7. MLFQS

MLFQS means:

> **Multi-Level Feedback Queue Scheduler**

This is a more advanced scheduler than ordinary priority scheduling.

Instead of manually assigning a fixed priority and relying on donation, MLFQS dynamically calculates priorities.

---

# 7.1 What is MLFQS?

MLFQS continuously evaluates:

```text
How much CPU is being used?
How many threads are competing?
How nice is the thread?
```

and calculates:

```text
recent_cpu
load_avg
priority
```

The basic relationship is:

```text
nice
  |
  v
recent_cpu
  |
  v
priority

System load
  |
  v
load_avg
  |
  v
recent_cpu calculation
```

---

# 7.2 Why MLFQS?

Ordinary priority scheduling:

```text
priority = manually assigned value
```

MLFQS:

```text
priority = dynamically calculated
```

This gives a more balanced scheduler.

A thread consuming lots of CPU should generally receive lower priority.

A thread waiting for CPU should eventually receive more CPU time.

---

# 7.3 Nice Value

Each thread has:

```c
int nice;
```

Nice represents how much the thread voluntarily affects its CPU priority.

Higher nice:

```text
more "nice" to other threads
       |
       v
lower priority
```

Lower nice:

```text
less nice
       |
       v
higher priority
```

Typical allowed range:

```text
-20 <= nice <= 20
```

---

# 7.4 Recent CPU

Each thread maintains:

```c
fixed_t recent_cpu;
```

This represents approximately how much CPU time the thread has recently consumed.

A CPU-intensive thread:

```text
recent_cpu ↑
```

generally receives:

```text
priority ↓
```

This prevents CPU-hungry threads from dominating the processor.

---

# 7.5 Load Average

The system maintains:

```c
fixed_t load_avg;
```

This represents the approximate number of threads competing for CPU time.

Conceptually:

```text
Few runnable threads
       |
       v
low load_avg

Many runnable threads
       |
       v
high load_avg
```

The load average is system-wide rather than per-thread.

---

# 7.6 Dynamic Priority

MLFQS calculates priority approximately as:

```text
priority = PRI_MAX
           - (recent_cpu / 4)
           - (nice * 2)
```

Therefore:

```text
higher recent_cpu -> lower priority
higher nice       -> lower priority
```

The result must be clamped to:

```text
PRI_MIN <= priority <= PRI_MAX
```

---

# 7.7 Required Functions

Important MLFQS functions include:

### `thread_set_nice()`

Changes the current thread's nice value.

Conceptually:

```c
thread_current()->nice = nice;
```

Then the thread's priority should be recalculated.

---

### `thread_get_nice()`

Returns:

```c
thread_current()->nice
```

---

### `thread_get_load_avg()`

Returns:

```text
100 * load_avg
```

The multiplication by 100 is required because the Pintos interface returns an integer representation.

---

### `thread_get_recent_cpu()`

Returns:

```text
100 * recent_cpu
```

Again, this converts the fixed-point value into the expected integer representation.

At one stage of development, these MLFQS functions were still placeholders, including `thread_set_nice()`, `thread_get_nice()`, `thread_get_load_avg()`, and `thread_get_recent_cpu()`.

---

# 8. Fixed-Point Arithmetic

MLFQS requires calculations involving fractional values.

The kernel generally avoids floating-point operations.

Therefore we use:

```text
fixed-point arithmetic
```

---

# 8.1 Why Fixed Point?

Suppose we need:

```text
load_avg = 1.5
```

A normal integer cannot store:

```text
1.5
```

Instead we store a scaled integer.

For example:

```text
1.5 * F
```

where `F` is a scaling factor.

---

# 8.2 Representation

A common Pintos representation uses:

```text
F = 2^14
```

Therefore:

```text
1.0 = 16384
2.0 = 32768
1.5 = 24576
```

The CPU still stores an integer.

We interpret that integer as a fixed-point value.

---

# 8.3 Conversion

Integer to fixed point:

```text
FP_FROM_INT(n)
```

Conceptually:

```text
n * F
```

Fixed point to integer:

```text
FP_TO_INT_ZERO(x)
```

Conceptually:

```text
x / F
```

For rounding:

```text
FP_TO_INT_NEAREST(x)
```

rounds instead of simply truncating.

---

# 8.4 Arithmetic

Addition:

```text
x + y
```

Subtraction:

```text
x - y
```

Fixed point multiplication:

```text
(x * y) / F
```

Fixed point division:

```text
(x * F) / y
```

Example:

```text
x = 1.5
y = 2.0

x * y = 3.0
```

Internally:

```text
(1.5F * 2.0F) / F
= 3.0F
```

---

# 9. Important Pintos Functions

This section summarizes the functions by responsibility.

## Thread Management

### `thread_current()`

Returns the currently running thread.

```text
CPU
 |
 v
current thread
```

---

### `thread_create()`

Creates a new thread and places it into the ready queue.

---

### `thread_block()`

Blocks the current thread.

```text
RUNNING -> BLOCKED
```

The function requires interrupts to be disabled before scheduling.

---

### `thread_unblock()`

Makes a blocked thread ready.

```text
BLOCKED -> READY
```

---

### `thread_yield()`

Gives up the CPU.

```text
RUNNING -> READY
```

---

### `schedule()`

Chooses the next thread to execute.

Conceptually:

```text
current thread
      |
      v
next_thread_to_run()
      |
      v
switch_threads()
      |
      v
new thread
```

The scheduler requires interrupts to be disabled and the current thread to have already transitioned away from `THREAD_RUNNING`.

---

# Timer Functions

### `timer_ticks()`

Returns the current timer tick.

---

### `timer_sleep()`

Requests that the current thread sleep.

---

### `timer_interrupt()`

Runs every timer tick.

Responsibilities include:

```text
increment ticks
check sleeping threads
wake expired threads
update scheduling statistics
```

---

# Priority Functions

### `thread_set_priority()`

Changes the base priority and recalculates effective priority.

---

### `thread_get_priority()`

Returns effective priority.

---

### `thread_yield()`

Allows a higher-priority ready thread to run.

---

# Synchronization Functions

### `lock_acquire()`

Acquires a lock.

With priority donation:

```text
check holder
     |
     v
donate if required
     |
     v
wait
     |
     v
acquire
```

---

### `lock_release()`

Releases a lock and removes donations associated with it.

---

### `sema_down()`

Waits for a semaphore.

---

### `sema_up()`

Signals a semaphore and wakes an appropriate waiter.

---

# MLFQS Functions

### `thread_set_nice()`

Changes nice value.

---

### `thread_get_nice()`

Returns nice value.

---

### `thread_get_load_avg()`

Returns:

```text
100 * load_avg
```

---

### `thread_get_recent_cpu()`

Returns:

```text
100 * recent_cpu
```

---

# 10. Files Modified / Related

The core files involved throughout the Threads work are:

```text
src/threads/thread.c
src/threads/thread.h

src/threads/timer.c
src/threads/timer.h

src/threads/synch.c
src/threads/synch.h
```

The build system and test infrastructure are also involved:

```text
src/threads/Makefile

src/tests/threads/
```

The saved build output confirms compilation of the main thread subsystem files, including:

```text
init.c
thread.c
interrupt.c
synch.c
timer.c
```

during the Threads build.

### `thread.c`

Main responsibilities:

```text
thread creation
thread blocking
thread unblocking
thread yielding
ready list
priority scheduling
priority donation support
MLFQS state
scheduler
```

Important functions:

```text
thread_create()
thread_block()
thread_unblock()
thread_yield()
thread_set_priority()
thread_get_priority()
thread_set_nice()
thread_get_nice()
thread_get_load_avg()
thread_get_recent_cpu()
thread_tick()
schedule()
```

---

### `thread.h`

Contains the thread structure and declarations.

Important state includes concepts such as:

```text
priority
base_priority
nice
recent_cpu
wake_tick
donations
waiting_lock
```

---

### `timer.c`

Main responsibilities:

```text
timer initialization
timer ticks
timer interrupts
timer_sleep()
```

The alarm-clock implementation depends on cooperation between `timer.c` and `thread.c`.

---

### `timer.h`

Contains timer-related declarations such as:

```c
timer_ticks()
timer_sleep()
```

---

### `synch.c`

Main responsibilities:

```text
semaphores
locks
condition variables
priority-aware synchronization
priority donation
```

Important functions:

```text
sema_down()
sema_up()

lock_init()
lock_acquire()
lock_try_acquire()
lock_release()

cond_wait()
cond_signal()
cond_broadcast()
```

Priority donation was integrated primarily around lock acquisition/release. The saved implementation explicitly tracks the lock holder, donation lock, waiting lock, and donation priority.

---

### `synch.h`

Contains declarations and synchronization structures.

---

### Tests

Alarm tests:

```text
alarm-single
alarm-multiple
alarm-simultaneous
alarm-priority
alarm-zero
alarm-negative
```

Priority tests:

```text
priority-change
priority-fifo
priority-preempt
priority-sema
priority-condvar
```

Donation tests:

```text
priority-donate-one
priority-donate-multiple
priority-donate-multiple2
priority-donate-nest
priority-donate-sema
priority-donate-lower
priority-donate-chain
```

MLFQS tests:

```text
mlfqs-load-1
mlfqs-load-60
mlfqs-load-avg
mlfqs-recent-1
mlfqs-fair-2
mlfqs-fair-20
mlfqs-nice-2
mlfqs-nice-10
mlfqs-block
```

---

# 11. Testing

The main command used is:

```bash
make check
```

It runs the complete Threads test suite.

Individual tests can also be executed.

For example:

```bash
pintos -v -k -T 60 --bochs -- -q run alarm-single
```

Priority donation:

```bash
pintos -v -k -T 60 --bochs -- -q run priority-donate-chain
```

MLFQS:

```bash
pintos -v -k -T 480 --bochs -- -q -mlfqs run mlfqs-load-1
```

The MLFQS tests require:

```text
-mlfqs
```

because they enable the MLFQS scheduler.

---

# 12. Debugging History

During development, a number of failures were encountered.

A major failure reported by the test runner was:

```text
Run didn't start up properly: no "Pintos booting" message
```

This appeared across alarm-clock, priority, donation, and MLFQS tests.

At one point the complete test suite reported:

```text
27 of 27 tests failed.
```

However, these failures were not necessarily 27 independent scheduling bugs.

The common:

```text
no "Pintos booting" message
```

indicated that the kernel was not successfully starting under the test command, so the first thing to investigate was the build/boot environment rather than changing every scheduling function.

---

# 13. Common Problems

## Problem 1 — Interrupts

Whenever manipulating shared scheduler lists:

```text
ready_list
sleep_list
donations
waiters
```

interrupt state must be considered carefully.

Typical pattern:

```c
enum intr_level old_level;

old_level = intr_disable();

/* critical section */

intr_set_level(old_level);
```

---

## Problem 2 — Calling `schedule()` incorrectly

` schedule()` expects:

```text
interrupts disabled
current thread not RUNNING
```

Therefore:

```text
disable interrupts
       |
       v
change state
       |
       v
schedule()
       |
       v
restore interrupts
```

---

## Problem 3 — Confusing base and effective priority

Do not think:

```text
priority = permanent priority
```

With donation:

```text
base_priority = original priority

priority = current effective priority
```

Example:

```text
base = 20
donation = 50

effective = 50
```

After donation disappears:

```text
effective = 20
```

---

## Problem 4 — Removing all donations on one lock release

Wrong:

```text
release lock
    |
    v
priority = base_priority
```

This can be incorrect if another donation is still active.

Correct:

```text
remove donations associated with released lock
        |
        v
find maximum remaining donation
        |
        v
priority = max(base, donations)
```

---

## Problem 5 — MLFQS and priority donation

MLFQS and priority donation represent different scheduling mechanisms.

Conceptually:

```text
Normal priority scheduler
        |
        +---- priority donation

MLFQS scheduler
        |
        +---- dynamic priority
             based on nice/recent_cpu
```

When MLFQS is enabled, priority donation is generally not used for scheduling decisions.

---

# 14. Final Conceptual Flow

The entire project can be understood as one progression.

## Stage 1 — Alarm Clock

We learned:

```text
How to block a thread
How to wake a thread
How timer interrupts work
How context switching works
```

Flow:

```text
timer_sleep()
     |
     v
thread_sleep()
     |
     v
BLOCKED
     |
     | timer interrupt
     v
thread_unblock()
     |
     v
READY
```

---

# Stage 2 — Priority Scheduling

We changed:

```text
"any ready thread"
```

into:

```text
"highest-priority ready thread"
```

Flow:

```text
ready_list
    |
    v
sort by priority
    |
    v
highest priority
    |
    v
CPU
```

---

# Stage 3 — Priority Donation

We solved:

```text
High -> waiting for Low
```

by temporarily changing:

```text
Low priority
```

to:

```text
High priority
```

Flow:

```text
High
 |
 | waits for lock
 v
Low
 |
 | receives donation
 v
High effective priority
```

---

# Stage 4 — Nested Donation

We extended donation through lock dependencies:

```text
H
|
v
M
|
v
L
```

so:

```text
H priority
   |
   v
M receives donation
   |
   v
L receives donation
```

---

# Stage 5 — MLFQS

Finally, scheduling becomes dynamic.

Instead of manually relying on:

```text
fixed priority
```

we calculate:

```text
load_avg
recent_cpu
nice
```

and use them to determine:

```text
dynamic priority
```

The overall relationship is:

```text
                 System
                   |
                   v
              load_avg
                   |
                   v
              recent_cpu
                   |
                   |
       +-----------+-----------+
       |                       |
       v                       v
     nice                 CPU usage
       |                       |
       +-----------+-----------+
                   |
                   v
              priority
                   |
                   v
               scheduler
```

---

# Quick Reference

| Feature              | Main Files            | Main Functions                                                                               |
| -------------------- | --------------------- | -------------------------------------------------------------------------------------------- |
| Alarm Clock          | `timer.c`, `thread.c` | `timer_sleep()`, `thread_sleep()`, `thread_unblock()`                                        |
| Thread Blocking      | `thread.c`            | `thread_block()`                                                                             |
| Thread Wake-up       | `thread.c`            | `thread_unblock()`                                                                           |
| Priority Scheduling  | `thread.c`            | `thread_yield()`, `thread_create()`                                                          |
| Priority Changes     | `thread.c`            | `thread_set_priority()`, `thread_get_priority()`                                             |
| Priority Donation    | `synch.c`, `thread.c` | `lock_acquire()`, `lock_release()`                                                           |
| Nested Donation      | `synch.c`, `thread.c` | `lock_acquire()`                                                                             |
| Multiple Donation    | `synch.c`, `thread.c` | `lock_release()`, priority recalculation                                                     |
| Semaphore Scheduling | `synch.c`             | `sema_down()`, `sema_up()`                                                                   |
| Condition Variables  | `synch.c`             | `cond_wait()`, `cond_signal()`                                                               |
| MLFQS                | `thread.c`, `timer.c` | `thread_set_nice()`, `thread_get_nice()`, `thread_get_load_avg()`, `thread_get_recent_cpu()` |
| Fixed Point          | MLFQS support         | conversion and arithmetic helpers                                                            |
| Testing              | `tests/threads/`      | alarm, priority, donation, MLFQS tests                                                       |

---

# Key Lessons

The most important concepts learned from this project are:

### 1. A sleeping thread should be blocked

```text
Sleeping != yielding repeatedly
```

A sleeping thread should not waste CPU.

### 2. The scheduler selects a ready thread

```text
BLOCKED
   |
   v
READY
   |
   v
scheduler
   |
   v
RUNNING
```

### 3. Priority is not always the base priority

With donation:

```text
effective priority
=
max(base priority, active donations)
```

### 4. Locks can create priority inversion

```text
High -> waits for Low
Medium -> consumes CPU
```

Priority donation fixes this.

### 5. Donation can propagate

```text
High -> Medium -> Low
```

The donation must follow the lock dependency chain.

### 6. MLFQS makes priority dynamic

```text
priority
   ^
   |
   +--- nice
   |
   +--- recent_cpu
   |
   +--- system load
```

### 7. Interrupt control is critical

Scheduler data structures are shared with interrupt-driven code.

Therefore:

```text
critical scheduler operation
        |
        v
disable interrupts
        |
        v
modify data
        |
        v
restore interrupts
```

---

# Development Status

The project was developed incrementally.

The saved development history confirms that:

* Alarm-clock functionality was implemented around a `sleep_list` and `wake_tick`.
* Priority-ordered ready-list scheduling was implemented.
* `thread_set_priority()` was extended to account for donations.
* Priority donation was implemented through locks.
* Donation records were associated with locks.
* Nested donation followed `waiting_lock` chains.
* Multiple donations were handled by selecting the maximum active donation.
* MLFQS tests were reached and executed.
* At an intermediate checkpoint, several MLFQS functions were still unimplemented and returned placeholder values.

Therefore, **MLFQS should be considered the final stage requiring verification rather than assuming that every MLFQS function in this document is already complete.**

---

# Final Mental Model

If you remember only one diagram, remember this:

```text
                    THREAD
                      |
          +-----------+-----------+
          |                       |
       SLEEP                   READY
          |                       |
          v                       v
     sleep_list             priority order
          |                       |
          |                       v
     timer interrupt          scheduler
          |                       |
          v                       v
       UNBLOCK                 RUNNING
                                  |
                                  |
                    +-------------+-------------+
                    |                           |
                  LOCK                       CPU usage
                    |                           |
                    v                           v
              DONATION                    recent_cpu
                    |                           |
                    v                           v
             effective priority             MLFQS
                    |                           |
                    +-------------+-------------+
                                  |
                                  v
                              SCHEDULER
```

This represents the progression of the Pintos Threads project:

```text
Alarm Clock
     ↓
Thread Blocking/Waking
     ↓
Priority Scheduling
     ↓
Priority Donation
     ↓
Nested/Multiple Donation
     ↓
Priority-Aware Synchronization
     ↓
MLFQS
     ↓
Dynamic CPU Scheduling
```

  


