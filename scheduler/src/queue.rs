//! Priority queues for the scheduler.

use crate::{MAX_PRIORITY, TaskId, TaskPriority};
use alloc::collections::VecDeque;

/// A simple run queue for a single priority level.
#[derive(Debug)]
pub struct RunQueue {
    /// Tasks in this queue (FIFO order).
    queue: VecDeque<TaskId>,
}

impl RunQueue {
    /// Create a new empty run queue.
    pub fn new() -> Self {
        Self {
            queue: VecDeque::new(),
        }
    }

    /// Add a task to the queue.
    pub fn push(&mut self, task_id: TaskId) {
        self.queue.push_back(task_id);
    }

    /// Remove and return the next task.
    pub fn pop(&mut self) -> Option<TaskId> {
        self.queue.pop_front()
    }

    /// Peek at the next task without removing.
    pub fn peek(&self) -> Option<TaskId> {
        self.queue.front().copied()
    }

    /// Remove a specific task from the queue.
    pub fn remove(&mut self, task_id: TaskId) -> bool {
        if let Some(pos) = self.queue.iter().position(|&id| id == task_id) {
            self.queue.remove(pos);
            true
        } else {
            false
        }
    }

    /// Check if queue is empty.
    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }

    /// Get queue length.
    pub fn len(&self) -> usize {
        self.queue.len()
    }
}

/// Priority queue with one run queue per priority level.
#[derive(Debug)]
pub struct PriorityQueue {
    /// Run queues indexed by priority level.
    queues: [RunQueue; MAX_PRIORITY],
}

impl PriorityQueue {
    /// Create a new empty priority queue.
    pub fn new() -> Self {
        Self {
            queues: [
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
                RunQueue::new(),
            ],
        }
    }

    /// Enqueue a task at the given priority.
    pub fn enqueue(&mut self, task_id: TaskId, priority: TaskPriority) {
        let idx = priority.0.min(MAX_PRIORITY - 1);
        self.queues[idx].push(task_id);
    }

    /// Dequeue the highest-priority task.
    pub fn dequeue(&mut self) -> Option<TaskId> {
        // Scan from highest to lowest priority
        for queue in self.queues.iter_mut().rev() {
            if let Some(task_id) = queue.pop() {
                return Some(task_id);
            }
        }
        None
    }

    /// Peek at the highest-priority task.
    pub fn peek(&self) -> Option<TaskId> {
        // Scan from highest to lowest priority
        for queue in self.queues.iter().rev() {
            if let Some(task_id) = queue.peek() {
                return Some(task_id);
            }
        }
        None
    }

    /// Remove a specific task from its queue.
    pub fn remove(&mut self, task_id: TaskId, priority: TaskPriority) -> bool {
        let idx = priority.0.min(MAX_PRIORITY - 1);
        self.queues[idx].remove(task_id)
    }

    /// Check if all queues are empty.
    pub fn is_empty(&self) -> bool {
        self.queues.iter().all(|q| q.is_empty())
    }

    /// Get total number of tasks across all queues.
    pub fn total_tasks(&self) -> usize {
        self.queues.iter().map(|q| q.len()).sum()
    }
}
