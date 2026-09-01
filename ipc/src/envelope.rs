//! Event envelope and catalog for the IPC system.

/// Event types for the system event bus.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EnvelopeType {
    /// Perception data ready.
    PerceptBundleReady,
    /// Planning result ready.
    PlanReady,
    /// Safety interlock triggered.
    SafetyInterlockTriggered,
    /// System startup.
    SystemStartup,
    /// System shutdown.
    SystemShutdown,
    /// Task created.
    TaskCreated(usize),
    /// Task exited.
    TaskExited(usize, i32),
    /// Memory pressure.
    MemoryPressure(u32),
    /// Timer tick.
    TimerTick(u64),
}

/// An event envelope for the system event bus.
#[derive(Debug, Clone)]
pub struct Envelope {
    /// Event type.
    pub event_type: EnvelopeType,
    /// Source task ID.
    pub source: usize,
    /// Timestamp.
    pub timestamp: u64,
    /// Event data (optional).
    pub data: [u8; 32],
}

impl Envelope {
    /// Create a new envelope.
    pub fn new(event_type: EnvelopeType, source: usize, timestamp: u64) -> Self {
        Self {
            event_type,
            source,
            timestamp,
            data: [0u8; 32],
        }
    }
}

/// Event catalog for type-safe event routing.
pub struct EventCatalog;

impl EventCatalog {
    /// Check if an event type is valid.
    pub fn is_valid_event(event: &EnvelopeType) -> bool {
        matches!(
            event,
            EnvelopeType::PerceptBundleReady
                | EnvelopeType::PlanReady
                | EnvelopeType::SafetyInterlockTriggered
                | EnvelopeType::SystemStartup
                | EnvelopeType::SystemShutdown
                | EnvelopeType::TaskCreated(_)
                | EnvelopeType::TaskExited(_, _)
                | EnvelopeType::MemoryPressure(_)
                | EnvelopeType::TimerTick(_)
        )
    }

    /// Get event priority (higher = more urgent).
    pub fn event_priority(event: &EnvelopeType) -> u8 {
        match event {
            EnvelopeType::SafetyInterlockTriggered => 255,
            EnvelopeType::SystemShutdown => 200,
            EnvelopeType::SystemStartup => 150,
            EnvelopeType::TimerTick(_) => 100,
            EnvelopeType::PerceptBundleReady => 80,
            EnvelopeType::PlanReady => 80,
            EnvelopeType::MemoryPressure(_) => 60,
            EnvelopeType::TaskCreated(_) => 40,
            EnvelopeType::TaskExited(_, _) => 40,
        }
    }
}
