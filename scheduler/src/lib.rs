//! # AEOS Scheduler Module
//!
//! Preemptive round-robin scheduler with priority support.
//!
//! ## Design
//! - Fixed-priority preemptive scheduling
//! - Round-robin within same priority level
//! - 8 priority levels (0 = idle, 7 = real-time)
//! - Time slicing via timer interrupt
//! - Task states: Ready, Running, Blocked, Suspended, Zombie
//!
//! ## Safety
//! Scheduler uses spinlocks for thread safety. Context switch
//! is architecture-specific and implemented in assembly.

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod task;
mod queue;
mod policy;

pub use task::{Task, TaskId, TaskState, TaskPriority, TaskContext};
pub use queue::{RunQueue, PriorityQueue};
pub use policy::{SchedulerPolicy, RoundRobin, PriorityRoundRobin};

/// Maximum number of tasks in the system.
pub const MAX_TASKS: usize = 64;

/// Default time slice in timer ticks.
pub const DEFAULT_TIME_SLICE: usize = 10;

/// Idle task priority.
pub const IDLE_PRIORITY: TaskPriority = TaskPriority(0);

/// Maximum priority level.
pub const MAX_PRIORITY: usize = 8;

/// Scheduler error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SchedulerError {
    /// Task table is full.
    TaskTableFull,
    /// Task ID is invalid.
    InvalidTaskId,
    /// Task is in wrong state for operation.
    InvalidTaskState,
    /// Priority is out of range.
    InvalidPriority,
    /// Scheduler is not initialized.
    NotInitialized,
}

/// The global scheduler state.
pub struct Scheduler {
    /// Task table.
    tasks: [Option<Task>; MAX_TASKS],
    /// Priority queues.
    queues: PriorityQueue,
    /// Currently running task.
    current: Option<TaskId>,
    /// Scheduler policy.
    policy: PriorityRoundRobin,
    /// Total context switches.
    context_switches: usize,
    /// Total ticks elapsed.
    ticks: usize,
    /// Whether scheduler is initialized.
    initialized: bool,
}

impl Scheduler {
    /// Create a new uninitialized scheduler.
    pub const fn new() -> Self {
        Self {
            tasks: [None; MAX_TASKS],
            queues: PriorityQueue::new(),
            current: None,
            policy: PriorityRoundRobin::new(),
            context_switches: 0,
            ticks: 0,
            initialized: false,
        }
    }

    /// Initialize the scheduler.
    pub fn init(&mut self) -> Result<(), SchedulerError> {
        if self.initialized {
            return Ok(());
        }

        // Create idle task
        let idle_task = Task::new_idle(TaskId(0));
        self.tasks[0] = Some(idle_task);
        self.queues.enqueue(TaskId(0), IDLE_PRIORITY);

        self.initialized = true;
        Ok(())
    }

    /// Create a new task.
    pub fn create_task(
        &mut self,
        name: &str,
        entry: usize,
        priority: TaskPriority,
        stack_size: usize,
    ) -> Result<TaskId, SchedulerError> {
        if !self.initialized {
            return Err(SchedulerError::NotInitialized);
        }

        if priority.0 >= MAX_PRIORITY {
            return Err(SchedulerError::InvalidPriority);
        }

        // Find free slot
        let id = self.find_free_slot().ok_or(SchedulerError::TaskTableFull)?;

        // Create task
        let task = Task::new(
            TaskId(id),
            name,
            entry,
            priority,
            stack_size,
        );

        self.tasks[id] = Some(task);
        self.queues.enqueue(TaskId(id), priority);

        Ok(TaskId(id))
    }

    /// Exit the current task.
    pub fn exit_task(&mut self, task_id: TaskId, exit_code: i32) -> Result<(), SchedulerError> {
        let task = self.get_task_mut(task_id)?;
        task.state = TaskState::Zombie;
        task.exit_code = exit_code;
        self.queues.remove(task_id, task.priority);
        Ok(())
    }

    /// Block a task (wait for event).
    pub fn block_task(&mut self, task_id: TaskId) -> Result<(), SchedulerError> {
        let task = self.get_task_mut(task_id)?;
        if task.state != TaskState::Running && task.state != TaskState::Ready {
            return Err(SchedulerError::InvalidTaskState);
        }
        task.state = TaskState::Blocked;
        self.queues.remove(task_id, task.priority);
        Ok(())
    }

    /// Unblock a task (event occurred).
    pub fn unblock_task(&mut self, task_id: TaskId) -> Result<(), SchedulerError> {
        let task = self.get_task_mut(task_id)?;
        if task.state != TaskState::Blocked {
            return Err(SchedulerError::InvalidTaskState);
        }
        task.state = TaskState::Ready;
        self.queues.enqueue(task_id, task.priority);
        Ok(())
    }

    /// Tick the scheduler (called from timer interrupt).
    pub fn tick(&mut self) -> Option<TaskId> {
        if !self.initialized {
            return None;
        }

        self.ticks += 1;

        // Decrement time slice
        if let Some(current) = self.current {
            if let Some(task) = &mut self.tasks[current.0] {
                if task.time_slice > 0 {
                    task.time_slice -= 1;
                }

                // If time slice expired, preempt
                if task.time_slice == 0 {
                    return self.schedule();
                }
            }
        } else {
            // No current task, schedule one
            return self.schedule();
        }

        None
    }

    /// Schedule the next task.
    pub fn schedule(&mut self) -> Option<TaskId> {
        let next = self.queues.peek()?;

        if let Some(current) = self.current {
            if current == next {
                return Some(current);
            }

            // Reset time slice for current task
            if let Some(task) = &mut self.tasks[current.0] {
                task.time_slice = self.policy.time_slice(task.priority);
                if task.state == TaskState::Running {
                    task.state = TaskState::Ready;
                    self.queues.enqueue(current, task.priority);
                }
            }

            self.context_switches += 1;
        }

        // Set new current task
        self.current = Some(next);
        if let Some(task) = &mut self.tasks[next.0] {
            task.state = TaskState::Running;
            task.time_slice = self.policy.time_slice(task.priority);
        }

        Some(next)
    }

    /// Get the current task ID.
    pub fn current_task(&self) -> Option<TaskId> {
        self.current
    }

    /// Get a task reference.
    pub fn get_task(&self, id: TaskId) -> Result<&Task, SchedulerError> {
        self.tasks
            .get(id.0)
            .and_then(|t| t.as_ref())
            .ok_or(SchedulerError::InvalidTaskId)
    }

    /// Get a mutable task reference.
    fn get_task_mut(&mut self, id: TaskId) -> Result<&mut Task, SchedulerError> {
        self.tasks
            .get_mut(id.0)
            .and_then(|t| t.as_mut())
            .ok_or(SchedulerError::InvalidTaskId)
    }

    /// Find a free slot in the task table.
    fn find_free_slot(&self) -> Option<usize> {
        self.tasks.iter().position(|t| t.is_none())
    }

    /// Get the number of context switches.
    pub fn context_switches(&self) -> usize {
        self.context_switches
    }

    /// Get the total ticks elapsed.
    pub fn ticks(&self) -> usize {
        self.ticks
    }
}

/// Global scheduler instance.
static mut SCHEDULER: Option<Scheduler> = None;

/// Get a reference to the global scheduler.
///
/// # Safety
/// Must be called after initialization and with interrupts disabled.
pub fn scheduler() -> &'static mut Scheduler {
    unsafe { SCHEDULER.get_or_insert_with(|| Scheduler::new()) }
}
