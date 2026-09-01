//! Scheduling policies.

use crate::{DEFAULT_TIME_SLICE, TaskPriority};

/// Scheduling policy trait.
pub trait SchedulerPolicy {
    /// Get the time slice for a task at the given priority.
    fn time_slice(&self, priority: TaskPriority) -> usize;

    /// Check if a task should be preempted.
    fn should_preempt(&self, current_priority: TaskPriority, new_priority: TaskPriority) -> bool;
}

/// Round-robin scheduling policy.
pub struct RoundRobin {
    /// Default time slice in ticks.
    default_slice: usize,
}

impl RoundRobin {
    /// Create a new round-robin policy.
    pub fn new() -> Self {
        Self {
            default_slice: DEFAULT_TIME_SLICE,
        }
    }

    /// Create with custom time slice.
    pub fn with_time_slice(slice: usize) -> Self {
        Self {
            default_slice: slice,
        }
    }
}

impl SchedulerPolicy for RoundRobin {
    fn time_slice(&self, _priority: TaskPriority) -> usize {
        self.default_slice
    }

    fn should_preempt(&self, _current: TaskPriority, _new: TaskPriority) -> bool {
        // Round-robin: always preempt on time slice expiry
        false
    }
}

/// Priority round-robin scheduling policy.
///
/// Higher priority tasks get longer time slices.
/// Same priority uses round-robin.
pub struct PriorityRoundRobin {
    /// Base time slice.
    base_slice: usize,
}

impl PriorityRoundRobin {
    /// Create a new priority round-robin policy.
    pub fn new() -> Self {
        Self {
            base_slice: DEFAULT_TIME_SLICE,
        }
    }
}

impl SchedulerPolicy for PriorityRoundRobin {
    fn time_slice(&self, priority: TaskPriority) -> usize {
        // Higher priority = longer time slice
        self.base_slice + (priority.0 * 2)
    }

    fn should_preempt(&self, current_priority: TaskPriority, new_priority: TaskPriority) -> bool {
        // Preempt if new task has higher priority
        new_priority > current_priority
    }
}

/// FIFO scheduling policy (no preemption).
pub struct Fifo;

impl SchedulerPolicy for Fifo {
    fn time_slice(&self, _priority: TaskPriority) -> usize {
        // FIFO tasks run until they yield or block
        usize::MAX
    }

    fn should_preempt(&self, _current: TaskPriority, _new: TaskPriority) -> bool {
        // FIFO: never preempt
        false
    }
}
