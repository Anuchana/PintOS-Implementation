# Pintos - Priority Scheduling

## Overview

This branch implements **priority scheduling** in the Pintos thread system.

The default Pintos scheduler does not consider thread priority when selecting the next thread. In this branch, the scheduler is modified so that threads with higher priorities receive preference over threads with lower priorities.

## Objectives

The priority scheduling implementation aims to:

- Maintain a priority value for every thread.
- Keep the ready list ordered by priority.
- Select the highest-priority ready thread.
- Correctly handle threads becoming ready after being blocked.
- Allow a thread's priority to be changed.
- Provide access to a thread's current priority.
- Ensure scheduler operations are protected from interrupt interference.

## Priority

Each thread has a priority value.

A larger priority value represents a higher priority.

For example:

```text
Priority 50 → Higher priority
Priority 40
Priority 30
Priority 20
Priority 10 → Lower priority
````

## Ready List

The `ready_list` contains threads that are ready to execute.

This branch maintains the ready list in **descending priority order**.

Example:

```text
             ready_list

        +----------------+
        | Thread B: 50   |
        +----------------+
                ↓
        +----------------+
        | Thread C: 30   |
        +----------------+
                ↓
        +----------------+
        | Thread A: 20   |
        +----------------+
```

The highest-priority thread is therefore located at the front of the ready list.

## Priority Comparison

A priority comparison function is used to determine the ordering of threads.

When threads are inserted into the ready list, their priorities are compared so that the list remains ordered.

For example:

```text
Before inserting Thread D:

Thread B (50)
Thread C (30)
Thread A (20)

Thread D = Priority 40

After insertion:

Thread B (50)
Thread D (40)
Thread C (30)
Thread A (20)
```

## Scheduler Behavior

When the scheduler needs to select a new thread, the highest-priority ready thread should be selected.

For example:

```text
Ready threads:

Thread A → Priority 20
Thread B → Priority 50
Thread C → Priority 30

Selected:

Thread B → Priority 50
```

This gives higher-priority threads preference for CPU execution.

## Thread Unblocking

When a blocked thread becomes ready, it is inserted into the ready list according to its priority.

The thread changes state from:

```text
THREAD_BLOCKED
       ↓
THREAD_READY
```

The thread does not immediately become the running thread merely because it was unblocked. It becomes eligible for scheduling.

## Priority Changes

The implementation supports changing a thread's priority.

When a thread's priority is changed, the scheduler must account for the new priority when determining which thread should run next.

If a thread lowers its priority and another ready thread has a higher priority, the higher-priority thread should receive preference.

## Getting Thread Priority

The implementation also provides functionality to obtain the current priority of a thread.

This allows other parts of the kernel and test cases to determine a thread's current scheduling priority.

## Yielding

When a running thread voluntarily yields the CPU, it becomes ready again and is placed into the ready list according to its priority.

This ensures that yielding does not break the priority ordering of the ready list.

Example:

```text
Running thread:
Thread A → Priority 30

Ready list:
Thread B → Priority 50
Thread C → Priority 20

After Thread A yields:

Thread B → Priority 50
Thread A → Priority 30
Thread C → Priority 20
```

The highest-priority ready thread is then preferred.

## Interrupt Protection

Scheduler data structures are modified with interrupts disabled.

This is important when:

* Adding threads to the ready list.
* Removing threads from the ready list.
* Changing thread scheduling state.
* Performing scheduling operations.

The previous interrupt state is restored after the critical operation.

This prevents interrupts from interfering with scheduler data structures while they are being modified.

## Thread State Transitions

The main thread states involved in priority scheduling are:

```text
                  +----------------+
                  |    RUNNING     |
                  +----------------+
                    ↓            ↑
                  yield       schedule
                    ↓            |
                  +----------------+
                  |     READY      |
                  +----------------+
                    ↓
               selected by
                scheduler
                    ↓
                  RUNNING


                  +----------------+
                  |     BLOCKED    |
                  +----------------+
                         |
                      unblock
                         ↓
                  +----------------+
                  |     READY      |
                  +----------------+
```

## Example Scheduling Scenario

Assume the following threads:

```text
Thread A → Priority 20
Thread B → Priority 50
Thread C → Priority 30
```

Initially:

```text
Ready List:

B (50) → C (30) → A (20)
```

The scheduler selects:

```text
B (50)
```

If B blocks while waiting for some event:

```text
Ready List:

C (30) → A (20)
```

When B becomes ready again:

```text
Ready List:

B (50) → C (30) → A (20)
```

B is placed back at the appropriate position based on its priority.

## Main Components Used

The priority scheduling implementation involves the following areas of the Pintos thread system:

| Component           | Purpose                                           |
| ------------------- | ------------------------------------------------- |
| Thread priority     | Stores the priority of each thread                |
| Ready list          | Maintains ready threads in priority order         |
| Priority comparison | Determines the ordering of threads                |
| Thread unblocking   | Adds blocked threads back to the ready list       |
| Thread yielding     | Re-inserts yielding threads according to priority |
| Priority setter     | Changes a thread's priority                       |
| Priority getter     | Retrieves a thread's priority                     |
| Scheduler           | Selects the next thread based on priority         |

## Expected Behavior

The implementation should satisfy the following rules:

1. Higher-priority threads are preferred over lower-priority threads.
2. The ready list remains ordered by priority.
3. Newly unblocked threads are inserted according to their priority.
4. Yielding threads are reinserted according to their priority.
5. Changing a thread's priority affects subsequent scheduling decisions.
6. Scheduler data structures are protected from interrupt interference.
7. A blocked thread cannot be selected until it becomes ready.

## Testing

The implementation can be tested using the Pintos thread scheduling tests.

Important areas to verify include:

* Basic priority scheduling.
* Priority changes.
* Priority-based yielding.
* Priority-based unblocking.
* Correct ready list ordering.
* Scheduler selection of the highest-priority thread.

## Branch Summary

This branch changes the Pintos scheduler from basic thread ordering to **priority-based scheduling**.

The main idea is:

```text
Higher Priority
       ↓
Higher Scheduling Preference
       ↓
Earlier Selection
```

The ready list and scheduling operations are modified to maintain this behavior throughout the thread lifecycle.
