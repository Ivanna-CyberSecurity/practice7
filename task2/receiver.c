#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// P (wait)
void wait_semaphore(int sem_id) {
    struct sembuf op;
    op.sem_num = 0; // index
    op.sem_op = -1; // operation
    op.sem_flg = SEM_UNDO;
    if (semop(sem_id, &op, 1) == -1) {
        perror("semop P failed");
        exit(EXIT_FAILURE);
    }
}

// V (signal)
void signal_semaphore(int sem_id) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = SEM_UNDO;
    if (semop(sem_id, &op, 1) == -1) {
        perror("semop V failed");
        exit(EXIT_FAILURE);
    }
}

int shared_memory_id;
void *shared_memory_ptr;
int semaphore_id;

void custom_signal_handler(int sig) {
    printf("Custom Client: Signal handler received signal %d\n", sig);
    if (sig == SIGUSR1) {
        int sum = 0, i = 0, val;

        printf("Custom Client: Locking semaphore before reading data...\n");
        wait_semaphore(semaphore_id);

        do {
            memcpy(&val, (int *)shared_memory_ptr + i, sizeof(int));
            if (val == 0) break;
            sum += val;
            i++;
        } while (1);

        memcpy(shared_memory_ptr, &sum, sizeof(int));

        printf("Custom Client: Releasing semaphore after reading...\n");
        signal_semaphore(semaphore_id);
        printf("Custom Client: Sending signal back to server...\n");
        kill(getppid(), SIGUSR1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <shared_memory_id> <semaphore_id>\n", argv[0]);
        return 1;
    }
    printf("Custom Client: Connecting to shared memory...\n");
    shared_memory_id = atoi(argv[1]);
    semaphore_id = atoi(argv[2]);

    shared_memory_ptr = shmat(shared_memory_id, NULL, 0);
    if (shared_memory_ptr == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    printf("Custom Client: Connecting to semaphore...\n");
    signal(SIGUSR1, custom_signal_handler);

    while (1) {
        pause();
    }

    shmdt(shared_memory_ptr);
    return 0;
}