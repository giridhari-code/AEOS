#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>

// CUDA GPU Kernel - Ye actual GPU par run hota hai
// AI operations like matrix multiplication ye karte hain
__global__ void vector_add(float *a, float *b, float *c, int n) {
    // Thread ID calculate karo - kernel se milta hai
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Boundary check
    if (idx < n) {
        // Ye computation GPU par parallel hota hai
        c[idx] = a[idx] + b[idx];

        // Print karo kaunsa thread kya kar raha hai
        if (idx < 5) {  // Sirf pehle 5 threads dikhao
            printf("  Thread %d: a[%d]=%.1f + b[%d]=%.1f = c[%d]=%.1f\n",
                   idx, idx, a[idx], idx, b[idx], idx, c[idx]);
        }
    }
}

// Matrix multiplication kernel - AI mein use hota hai
__global__ void matrix_multiply(float *A, float *B, float *C, int M, int N, int K) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

    if (row < M && col < N) {
        float sum = 0.0f;
        for (int k = 0; k < K; k++) {
            sum += A[row * K + k] * B[k * N + col];
        }
        C[row * N + col] = sum;
    }
}

void check_cuda_error(const char* msg) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("CUDA Error: %s - %s\n", msg, cudaGetErrorString(err));
        exit(1);
    }
}

int main() {
    printf("CUDA GPU KERNEL DEMO\n");
    printf("Ye code dikhata hai kaise AI operations GPU par run hote hain\n\n");

    // Check available GPUs
    int device_count;
    cudaGetDeviceCount(&device_count);
    printf("Available GPUs: %d\n", device_count);

    if (device_count == 0) {
        printf("No CUDA GPU found! CPU mode me chal raha hai.\n");
        return 1;
    }

    // Set device
    cudaSetDevice(0);

    // Get GPU info
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("GPU Name: %s\n", prop.name);
    printf("Compute Capability: %d.%d\n", prop.major, prop.minor);
    printf("Total Global Memory: %lu MB\n", prop.totalGlobalMem / (1024*1024));
    printf("Max Threads per Block: %d\n", prop.maxThreadsPerBlock);

    // ============================================
    // DEMO 1: Vector Addition (Simple AI Operation)
    // ============================================
    printf("\n========================================\n");
    printf("1. VECTOR ADDITION (Basic AI Operation)\n");
    printf("========================================\n");

    int n = 20;
    size_t size = n * sizeof(float);

    // Host memory allocate karo (CPU RAM)
    float *h_a = (float*)malloc(size);
    float *h_b = (float*)malloc(size);
    float *h_c = (float*)malloc(size);

    // Initialize vectors
    for (int i = 0; i < n; i++) {
        h_a[i] = i * 1.0f;
        h_b[i] = i * 2.0f;
    }

    // Device memory allocate karo (GPU VRAM)
    float *d_a, *d_b, *d_c;
    cudaMalloc(&d_a, size);
    cudaMalloc(&d_b, size);
    cudaMalloc(&d_c, size);

    check_cuda_error("Memory allocation");

    // CPU se GPU me data copy karo (Kernel ke through)
    printf("\nCopying data from CPU to GPU...\n");
    cudaMemcpy(d_a, h_a, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, size, cudaMemcpyHostToDevice);

    // Kernel launch karo - ye GPU par run hota hai
    printf("Launching CUDA kernel...\n\n");

    int threads_per_block = 256;
    int blocks = (n + threads_per_block - 1) / threads_per_block;

    vector_add<<<blocks, threads_per_block>>>(d_a, d_b, d_c, n);
    check_cuda_error("Kernel launch");

    // Wait for GPU to finish
    cudaDeviceSynchronize();

    // GPU se CPU me result copy karo
    printf("\nCopying result from GPU to CPU...\n");
    cudaMemcpy(h_c, d_c, size, cudaMemcpyDeviceToHost);

    // Print first 10 results
    printf("\nResults (first 10):\n");
    for (int i = 0; i < 10; i++) {
        printf("  c[%d] = %.1f\n", i, h_c[i]);
    }

    // Cleanup
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    free(h_a);
    free(h_b);
    free(h_c);

    // ============================================
    // DEMO 2: Matrix Multiplication (AI Core Op)
    // ============================================
    printf("\n========================================\n");
    printf("2. MATRIX MULTIPLICATION (AI Core Operation)\n");
    printf("========================================\n");

    int M = 4, N = 4, K = 4;
    size_t mat_size = M * N * sizeof(float);

    float *h_A = (float*)malloc(mat_size);
    float *h_B = (float*)malloc(mat_size);
    float *h_C = (float*)malloc(mat_size);

    // Initialize matrices
    for (int i = 0; i < M * K; i++) h_A[i] = i * 0.1f;
    for (int i = 0; i < K * N; i++) h_B[i] = i * 0.2f;

    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, mat_size);
    cudaMalloc(&d_B, mat_size);
    cudaMalloc(&d_C, mat_size);

    cudaMemcpy(d_A, h_A, mat_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, mat_size, cudaMemcpyHostToDevice);

    dim3 block_size(16, 16);
    dim3 grid_size((N + 15) / 16, (M + 15) / 16);

    printf("\nLaunching matrix multiplication kernel...\n");
    matrix_multiply<<<grid_size, block_size>>>(d_A, d_B, d_C, M, N, K);
    check_cuda_error("Matrix multiply kernel");

    cudaDeviceSynchronize();

    cudaMemcpy(h_C, d_C, mat_size, cudaMemcpyDeviceToHost);

    printf("\nResult Matrix C (partial):\n");
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            printf("  C[%d][%d] = %.2f\n", i, j, h_C[i * N + j]);
        }
    }

    // Cleanup
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
    free(h_A);
    free(h_B);
    free(h_C);

    printf("\n========================================\n");
    printf("KEY CONCEPTS:\n");
    printf("========================================\n");
    printf("1. CPU (Host) <-> GPU (Device) communication\n");
    printf("2. Memory allocation: cudaMalloc (GPU VRAM)\n");
    printf("3. Data transfer: cudaMemcpy (PCIe bus)\n");
    printf("4. Kernel launch: <<<blocks, threads>>>\n");
    printf("5. Parallel execution: Thousands of threads\n");

    return 0;
}
