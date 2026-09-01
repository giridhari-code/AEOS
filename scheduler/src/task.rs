//! Task management.

use core::sync::atomic::{AtomicUsize, Ordering};

/// Unique task identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TaskId(pub usize);

/// Task state.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TaskState {
    /// Task is ready to run.
    Ready,
    /// Task is currently running.
    Running,
    /// Task is blocked (waiting for event).
    Blocked,
    /// Task is suspended (not scheduled).
    Suspended,
    /// Task has exited.
    Zombie,
}

/// Task priority (0 = idle, 7 = real-time).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TaskPriority(pub usize);

impl TaskPriority {
    /// Create a new priority level.
    pub const fn new(level: usize) -> Self {
        Self(level)
    }

    /// Get the priority level.
    pub const fn level(&self) -> usize {
        self.0
    }
}

/// CPU register context for context switching.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TaskContext {
    /// Stack pointer.
    pub sp: usize,
    /// Program counter.
    pub pc: usize,
    /// General purpose registers (ARM64: x0-x30).
    pub regs: [usize; 32],
    /// Floating point registers (if needed).
    pub fp_regs: [u128; 32],
    /// SIMD/NEON registers.
    pub simd_regs: [u128; 32],
}

impl TaskContext {
    /// Create a new context for a task entry point.
    pub fn new(entry: usize, stack_top: usize) -> Self {
        let mut ctx = Self {
            sp: stack_top,
            pc: entry,
            regs: [0; 32],
            fp_regs: [0u128; 32],
            simd_regs: [0u128; 32],
        };
        // Set up initial stack frame
        ctx.regs[13] = stack_top; // SP
        ctx.regs[14] = entry;    // LR (link register)
        ctx
    }
}

/// A task control block.
#[derive(Debug)]
pub struct Task {
    /// Unique task ID.
    pub id: TaskId,
    /// Task name.
    pub name: &'static str,
    /// Current state.
    pub state: TaskState,
    /// Task priority.
    pub priority: TaskPriority,
    /// CPU context.
    pub context: TaskContext,
    /// Time slice remaining (in ticks).
    pub time_slice: usize,
    /// Stack base address.
    pub stack_base: usize,
    /// Stack size in bytes.
    pub stack_size: usize,
    /// Exit code (valid when state = Zombie).
    pub exit_code: i32,
    /// Parent task ID.
    pub parent: Option<TaskId>,
    /// Creation tick.
    pub created_at: usize,
    /// Total CPU time used.
    pub cpu_time: usize,
}

/// Global task ID counter.
static NEXT_TASK_ID: AtomicUsize = AtomicUsize::new(1);

impl Task {
    /// Create a new task.
    pub fn new(
        id: TaskId,
        name: &'static str,
        entry: usize,
        priority: TaskPriority,
        stack_size: usize,
    ) -> Self {
        // Allocate stack (would use frame allocator in real implementation)
        let stack_base = 0x4000_0000 + id.0 * stack_size;
        let stack_top = stack_base + stack_size;

        Self {
            id,
            name,
            state: TaskState::Ready,
            priority,
            context: TaskContext::new(entry, stack_top),
            time_slice: 10,
            stack_base,
            stack_size,
            exit_code: 0,
            parent: None,
            created_at: 0,
            cpu_time: 0,
        }
    }

    /// Create the idle task.
    pub fn new_idle(id: TaskId) -> Self {
        Self {
            id,
            name: "idle",
            state: TaskState::Ready,
            priority: TaskPriority(0),
            context: TaskContext::new(0, 0),
            time_slice: usize::MAX,
            stack_base: 0,
            stack_size: 0,
            exit_code: 0,
            parent: None,
            created_at: 0,
            cpu_time: 0,
        }
    }

    /// Check if task is alive (not zombie or exited).
    pub fn is_alive(&self) -> bool {
        self.state != TaskState::Zombie
    }

    /// Get the next available task ID.
    pub fn next_id() -> TaskId {
        TaskId(NEXT_TASK_ID.fetch_add(1, Ordering::Relaxed))
    }
}
