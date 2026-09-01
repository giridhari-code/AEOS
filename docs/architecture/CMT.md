# Consciousness Manifold Theory (CMT)

> **Core Formula:** `C = E_f`

CMT models consciousness as a manifold transformation process where
multiple input streams are mapped into a unified embedded experience.

## Overview

Consciousness (C) emerges from Embedded Experience (E_f), which is
the result of transforming high-dimensional input manifolds through
integration, selection, compression, and mapping operations.

## Input Parameters

| Symbol | Name | Description |
|--------|------|-------------|
| **P** | Perception | Sensory data from environment (vision, audio, touch) |
| **M** | Memory | Stored experiences, learned patterns, context |
| **B** | Body State | Proprioception, interoception, physiological signals |
| **S** | Self-Model | Internal representation of self, capabilities, limits |
| **A** | Action | Motor output, behavioral history, intention signals |
| **...** | Other Inputs | Context, social signals, environmental factors |

## Key Components

### IM - Input Manifold

High-dimensional space where all input streams converge. Each input
is a point on this manifold, carrying its own coordinate system.

### T - Manifold Transformation

The core processing operation:

- **Integration**: Merge disparate input streams into unified space
- **Selection**: Attend to relevant features, suppress noise
- **Compression**: Reduce dimensionality while preserving meaning
- **Mapping**: Transform to embedded experience space

### E_f - Embedded Experience

The compressed, integrated representation of all inputs at time t.
This is the "what it is like" moment - the qualitative character
of the current state.

### C - Consciousness

Unified subjective experience emerging from E_f. Not a thing but
a process - the ongoing manifold transformation itself.

## Mathematical Formulation

```
C(t) = E_f(t) = T(IM(P(t), M(t), B(t), S(t), A(t), ...))

Where:
  IM: R^(n1) x R^(n2) x ... x R^(nk) -> R^d  (input manifold)
  T:  R^d -> R^m                               (transformation)
  E_f: R^m                                     (embedded experience)
  C:  Process(E_f)                              (consciousness)
```

## AEOS Implementation

In AEOS, CMT maps to the kernel AI engine:

| CMT | AEOS |
|-----|------|
| P (Perception) | `perception/` subsystem |
| M (Memory) | `memory/` kernel module |
| B (Body State) | `hal/` sensor readings |
| S (Self-Model) | `kernel/ai.c` state tracking |
| A (Action) | `planning/` + `robotics/` outputs |
| IM | Input vector construction in `ai_perceive()` |
| T | MLP forward pass in `ai_decide()` |
| E_f | Hidden layer activations |
| C | Output action selection |

## Diagrams

See [cmt-diagram.mmd](cmt-diagram.mmd) for the Mermaid flowchart.
See [cmt-prompts.md](cmt-prompts.md) for AI image generation prompts.
