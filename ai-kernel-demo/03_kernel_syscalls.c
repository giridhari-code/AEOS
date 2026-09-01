#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>

// ============================================
// AI-KERNEL INTERACTION LOW LEVEL DEMO
// Ye code dikhata hai actual system calls kaise kaam karti hain
// ============================================

// NVIDIA GPU ioctl definitions (simplified)
#define NVIDIA_IOCTL_MAGIC 'N'
#define NVIDIA_GET_VERSION _IOW(NVIDIA_IOCTL_MAGIC, 0, int)
#define NVIDIA_GET_INFO _IOR(NVIDIA_IOCTL_MAGIC, 1, struct nvidia_info)

struct nvidia_info {
    int gpu_count;
    int driver_version;
    char gpu_name[256];
};

// System call wrapper - kernel se seedha baat
long get_system_call_demo(void) {
    printf("SYSTEM CALLS DEMO - AI KERNEL INTERACTION\n");
    printf("==========================================\n\n");

    // 1. getpid - Process ID
    printf("1. GETPID SYSCALL (Process Identification)\n");
    printf("   syscall(SYS_getpid) = %d\n", getpid());
    printf("   AI Framework apna process identify karta hai\n\n");

    // 2. getuid - User ID
    printf("2. GETUID SYSCALL (User Identification)\n");
    printf("   syscall(SYS_getuid) = %d\n", getuid());
    printf("   AI checks karta hai user ka permission\n\n");

    // 3. sysinfo - System information
    printf("3. SYSINFO SYSCALL (System Resources)\n");
    printf("   CPU cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    printf("   Page size: %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("   AI hardware resources check karta hai\n\n");

    return 0;
}

// Memory mapping - AI models ko memory mein load karna
void memory_mapping_demo(void) {
    printf("4. MMAP SYSCALL (Memory Mapping for AI Models)\n");
    printf("===============================================\n");

    size_t size = 1024 * 1024;  // 1 MB

    // mmap se memory allocate karo - kernel ke through
    void *ptr = mmap(
        NULL,           // Address (kernel choose karega)
        size,           // Size
        PROT_READ | PROT_WRITE,  // Permissions
        MAP_PRIVATE | MAP_ANONYMOUS,  // Flags
        -1,             // File descriptor
        0               // Offset
    );

    if (ptr == MAP_FAILED) {
        printf("   mmap failed: %s\n", strerror(errno));
        return;
    }

    printf("   Allocated %zu bytes via mmap\n", size);
    printf("   Address: %p\n", ptr);
    printf("   AI framework isme model weights store karta hai\n\n");

    // Simulate writing model weights
    float *weights = (float*)ptr;
    for (int i = 0; i < 10; i++) {
        weights[i] = i * 0.12345f;
    }

    printf("   First 5 'model weights':\n");
    for (int i = 0; i < 5; i++) {
        printf("     weights[%d] = %.5f\n", i, weights[i]);
    }

    // Cleanup
    munmap(ptr, size);
    printf("\n");
}

// File operations - AI data files access karna
void file_operations_demo(void) {
    printf("5. FILE OPERATIONS (AI Data Loading)\n");
    printf("=====================================\n");

    // Open file - kernel ke through
    int fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd == -1) {
        printf("   Failed to open: %s\n", strerror(errno));
        return;
    }

    printf("   Opened /proc/cpuinfo (fd=%d)\n", fd);
    printf("   AI datasets ko file system se load karta hai\n");

    // Read first 200 bytes
    char buffer[201];
    ssize_t bytes_read = read(fd, buffer, 200);
    buffer[bytes_read] = '\0';

    printf("\n   CPU Info (first 200 chars):\n");
    printf("   ---------------------------\n");

    // Print line by line
    char *line = strtok(buffer, "\n");
    int count = 0;
    while (line && count < 5) {
        printf("   %s\n", line);
        line = strtok(NULL, "\n");
        count++;
    }

    close(fd);
    printf("\n");
}

// Process control - AI training processes
void process_control_demo(void) {
    printf("6. PROCESS CONTROL (AI Training Jobs)\n");
    printf("=====================================\n");

    printf("   Current PID: %d\n", getpid());
    printf("   Parent PID: %d\n", getppid());

    // Getrusage - Resource usage
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    printf("   CPU time used: %ld.%06ld seconds\n",
           usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("   Memory usage: %ld KB\n", usage.ru_maxrss);
    printf("   AI monitors resource usage for training\n\n");

    // Getrlimit - Resource limits
    struct rlimit limit;

    getrlimit(RLIMIT_AS, &limit);
    printf("   Address space limit: %ld MB\n", limit.rlim_cur / (1024*1024));

    getrlimit(RLIMIT_NOFILE, &limit);
    printf("   Max open files: %ld\n", limit.rlim_cur);

    printf("   AI checks resource limits before training\n\n");
}

// Shared memory - Multi-GPU communication
void shared_memory_demo(void) {
    printf("7. SHARED MEMORY (Multi-GPU Communication)\n");
    printf("===========================================\n");

    // Create shared memory segment
    int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0666);
    if (shmid == -1) {
        printf("   shmget failed: %s\n", strerror(errno));
        return;
    }

    // Attach to process
    void *ptr = shmat(shmid, NULL, 0);
    if (ptr == (void*)-1) {
        printf("   shmat failed: %s\n", strerror(errno));
        return;
    }

    printf("   Created shared memory segment (id=%d)\n", shmid);
    printf("   Address: %p\n", ptr);
    printf("   AI uses this for multi-GPU gradient sync\n\n");

    // Cleanup
    shmdt(ptr);
    shmctl(shmid, IPC_RMID, NULL);
}

// Pipe operations - AI inference pipeline
void pipe_demo(void) {
    printf("8. PIPE SYSCALL (AI Inference Pipeline)\n");
    printf("=======================================\n");

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("   pipe failed: %s\n", strerror(errno));
        return;
    }

    printf("   Created pipe (read_fd=%d, write_fd=%d)\n",
           pipefd[0], pipefd[1]);
    printf("   AI uses pipes for data flow between processes\n\n");

    // Write to pipe
    const char *message = "AI inference request";
    write(pipefd[1], message, strlen(message) + 1);

    // Read from pipe
    char buffer[256];
    read(pipefd[0], buffer, sizeof(buffer));
    printf("   Received: %s\n\n", buffer);

    close(pipefd[0]);
    close(pipefd[1]);
}

int main(void) {
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║  AI-KERNEL INTERACTION - LOW LEVEL DEMO           ║\n");
    printf("║  Ye code dikhata hai actual system calls          ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");

    get_system_call_demo();
    memory_mapping_demo();
    file_operations_demo();
    process_control_demo();
    shared_memory_demo();
    pipe_demo();

    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY: AI-KERNEL INTERACTION                    ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║  1. System calls = Kernel interface                ║\n");
    printf("║  2. mmap = Large model memory management           ║\n");
    printf("║  3. File ops = Dataset loading                     ║\n");
    printf("║  4. Pipes = Inference pipeline                     ║\n");
    printf("║  5. Shared memory = Multi-GPU sync                 ║\n");
    printf("║  6. Process control = Training job management      ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");

    return 0;
}
