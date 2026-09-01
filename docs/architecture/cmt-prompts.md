# CMT Diagram Prompts

Prompts for generating visual representations of the
Consciousness Manifold Theory (CMT) diagram.

## 1. Text-to-Image AI Prompt (Midjourney / DALL-E / Ideogram)

```
Create a clean, modern, high-resolution infographic vector diagram
titled "CMT (Consciousness Manifold Theory)". Light background,
soft pastel color-coded rounded boxes, and crisp typography.

Top Center: "Core Principle" header with math equation C = E_f
inside a rounded border.

Left Side: 6 stacked rounded pill buttons with icons:
  - blue "P - Perception"
  - green "M - Memory"
  - orange "B - Body State"
  - pink "S - Self-Model"
  - purple "A - Action"
  - grey "... Other Inputs"

Center Flow: Arrows leading from all inputs into a glowing, organic
high-dimensional cloud node marked "IM Input Manifold", followed by
an arrow to a green box "T Manifold Transformation", then an arrow
to a yellow circle with a brain icon marked "E_f Embedded Experience",
and finally an arrow to a pink box "C Consciousness (Unified Experience)".

Bottom Section: Three distinct legend panels explaining the input
parameters, key components, and core equation clearly.

Style: Flat design, minimal shadows, professional technical diagram,
4K resolution, white background.
```

## 2. Mermaid.js Flowchart

See [cmt-diagram.mmd](cmt-diagram.mmd) for the renderable code.

Render with:
```bash
# CLI
mmdc -i cmt-diagram.mmd -o cmt-diagram.svg

# Or use Mermaid Live Editor
# https://mermaid.live
```

## 3. Content Summary Prompt (For AI Explanation / Analysis)

```
Analyze and explain the Consciousness Manifold Theory (CMT) diagram
based on the core formula C = E_f.

Detail how multiple inputs (Perception, Memory, Body State,
Self-Model, Action, and Context) pass through the Input Manifold (IM),
undergo Manifold Transformation (T) via integration and compression,
and result in the Embedded Experience (E_f) to form unified
Consciousness (C).

Context: This theory is implemented in the AEOS kernel AI engine
as a 3-8-3 MLP neural network running in Q16.16 fixed-point arithmetic.
```

## 4. LaTeX Diagram (TikZ)

```latex
\documentclass[border=10pt]{standalone}
\usepackage{tikz}
\usetikzlibrary{shapes.geometric, arrows.meta, positioning}

\begin{document}
\begin{tikzpicture}[
    node distance=1.5cm,
    input/.style={rectangle, rounded corners, fill=blue!20, minimum width=2cm},
    process/.style={rectangle, fill=green!20, minimum width=2.5cm},
    manifold/.style={cloud, fill=cyan!20, minimum width=3cm, minimum height=1.5cm},
    output/.style={circle, fill=yellow!20, minimum size=2cm},
    result/.style={rectangle, fill=pink!20, minimum width=2.5cm},
    arrow/.style={-{Stealth[length=3mm]}, thick}
]

% Inputs
\node[input] (P) {P - Perception};
\node[input, below=0.3cm of P] (M) {M - Memory};
\node[input, below=0.3cm of M] (B) {B - Body State};
\node[input, below=0.3cm of B] (S) {S - Self-Model};
\node[input, below=0.3cm of S] (A) {A - Action};
\node[input, below=0.3cm of A] (O) {... Other};

% Core flow
\node[manifold, right=2cm of M] (IM) {IM\\Input Manifold};
\node[process, right=2cm of IM] (T) {T\\Transformation};
\node[output, right=2cm of T] (Ef) {E$_f$};
\node[result, right=2cm of Ef] (C) {C\\Consciousness};

% Arrows
\foreach \i in {P,M,B,S,A,O}
    \draw[arrow] (\i) -- (IM);
\draw[arrow] (IM) -- (T);
\draw[arrow] (T) -- (Ef);
\draw[arrow] (Ef) -- (C);

% Title
\node[above=0.5cm of IM, font=\Large\bfseries] {CMT: C = E$_f$};

\end{tikzpicture}
\end{document}
```

## 5. HTML-CSS Diagram

```html
<!DOCTYPE html>
<html>
<head>
<style>
  .cmt-container {
    display: flex;
    align-items: center;
    gap: 20px;
    font-family: system-ui, sans-serif;
    padding: 40px;
    background: #fafafa;
    border-radius: 12px;
  }
  .cmt-inputs {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .cmt-input {
    padding: 10px 20px;
    border-radius: 20px;
    color: white;
    font-weight: 600;
    min-width: 140px;
    text-align: center;
  }
  .cmt-arrow { font-size: 24px; color: #666; }
  .cmt-manifold {
    padding: 20px 30px;
    background: #e3f2fd;
    border: 2px solid #1565c0;
    border-radius: 12px;
    text-align: center;
  }
  .cmt-transform {
    padding: 20px 30px;
    background: #e8f5e9;
    border: 2px solid #2e7d32;
    border-radius: 12px;
    text-align: center;
  }
  .cmt-experience {
    width: 80px; height: 80px;
    background: #fff8e1;
    border: 2px solid #f9a825;
    border-radius: 50%;
    display: flex; align-items: center; justify-content: center;
    font-weight: bold;
  }
  .cmt-consciousness {
    padding: 20px 30px;
    background: #fce4ec;
    border: 2px solid #c62828;
    border-radius: 12px;
    text-align: center;
    font-weight: bold;
  }
</style>
</head>
<body>
<div class="cmt-container">
  <div class="cmt-inputs">
    <div class="cmt-input" style="background:#2196f3">P Perception</div>
    <div class="cmt-input" style="background:#4caf50">M Memory</div>
    <div class="cmt-input" style="background:#ff9800">B Body State</div>
    <div class="cmt-input" style="background:#e91e63">S Self-Model</div>
    <div class="cmt-input" style="background:#9c27b0">A Action</div>
    <div class="cmt-input" style="background:#9e9e9e">... Other</div>
  </div>
  <div class="cmt-arrow">&#10132;</div>
  <div class="cmt-manifold"><b>IM</b><br>Input Manifold</div>
  <div class="cmt-arrow">&#10132;</div>
  <div class="cmt-transform"><b>T</b><br>Transformation</div>
  <div class="cmt-arrow">&#10132;</div>
  <div class="cmt-experience"><b>E&#8320;</b></div>
  <div class="cmt-arrow">&#10132;</div>
  <div class="cmt-consciousness"><b>C</b><br>Consciousness</div>
</div>
</body>
</html>
```

## 6. AEOS Kernel Mapping

```
CMT Theory          AEOS Implementation
─────────────       ──────────────────────────────────
P (Perception)  →   perception/vision/audio subsystems
M (Memory)      →   memory/ kernel module + storage/
B (Body State)  →   hal/ sensor readings + device/
S (Self-Model)  →   kernel/ai.c state tracking
A (Action)      →   planning/ + robotics/ outputs
IM              →   ai_perceive() input vector
T               →   ai_decide() MLP forward pass
E_f             →   hidden layer activations (Q16.16)
C               →   output action selection
```
