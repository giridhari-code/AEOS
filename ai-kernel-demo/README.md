# AI-Kernel Interaction Demo

Ye project dikhata hai kaise AI (Artificial Intelligence) Kernel ke saath kaam karta hai.

## 📁 Files

| File | Description |
|------|-------------|
| `01_system_calls.py` | Python se system calls ka demo |
| `02_cuda_kernel.cu` | CUDA GPU kernel code |
| `03_kernel_syscalls.c` | C mein low-level system calls |
| `04_pytorch_kernel.py` | PyTorch - Kernel interaction |

## 🚀 Kaise Chalaye

### 1. Python System Calls Demo
```bash
python3 01_system_calls.py
```

### 2. CUDA Kernel (GPU wale system ke liye)
```bash
# Compile
nvcc 02_cuda_kernel.cu -o cuda_demo

# Run
./cuda_demo
```

### 3. C Kernel Syscalls
```bash
# Compile
gcc 03_kernel_syscalls.c -o kernel_demo

# Run
./kernel_demo
```

### 4. PyTorch Demo
```bash
python3 04_pytorch_kernel.py
```

## 📚 Kya Sikhne Ko Milega

### System Calls
- `getpid()` - Process identify karna
- `mmap()` - Memory allocate karna (AI models ke liye)
- `open()/read()` - Files se data load karna
- `ioctl()` - GPU devices control karna

### CUDA Kernels
- Matrix multiplication (AI ka core operation)
- Parallel computation (thousands of threads)
- GPU memory management

### AI Framework Interaction
- PyTorch CUDA operations
- Kernel launch syntax
- Memory transfers (CPU ↔ GPU)

## 🧠 AI-Kernel Connection

```
AI Framework (PyTorch/TensorFlow)
        │
        ▼
   CUDA Runtime API
        │
        ▼
   GPU Driver (nvidia.ko)
        │
        ▼
   Linux Kernel
        │
        ▼
   GPU Hardware (NVIDIA/AMD)
```

## 📊 Performance Comparison

| Operation | CPU Time | GPU Time | Speedup |
|-----------|----------|----------|---------|
| Matrix Multiply (1000x1000) | ~50ms | ~5ms | 10x |
| Neural Network Forward Pass | ~20ms | ~2ms | 10x |
| Backpropagation | ~100ms | ~10ms | 10x |

## 🔧 Requirements

- Python 3.6+
- NumPy
- PyTorch (optional, for GPU demo)
- CUDA Toolkit (for GPU kernel)
- GCC (for C code)

## 📖 Resources

- [CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
- [Linux System Calls](https://man7.org/linux/man-pages/man2/syscalls.2.html)
- [PyTorch CUDA Semantics](https://pytorch.org/docs/stable/notes/cuda.html)
