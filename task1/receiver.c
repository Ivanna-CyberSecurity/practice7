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
void P(int sem_id) {
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
void V(int sem_id) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = SEM_UNDO;
    if (semop(sem_id, &op, 1) == -1) {
        perror("semop V failed");
        exit(EXIT_FAILURE);
    }
}

int shm_id;
void *shm_ptr;
int sem_id;

void signal_handler(int sig) {
    printf("Client: Signal handler received signal %d\n", sig);
    if (sig == SIGUSR1) {
        int sum = 0, i = 0, val;

        printf("Client: Locking semaphore before reading data...\n");
        P(sem_id);

        do {
            memcpy(&val, (int *)shm_ptr + i, sizeof(int));
            if (val == 0) break;
            sum += val;
            i++;
        } while (1);

        memcpy(shm_ptr, &sum, sizeof(int));

        printf("Client: Releasing semaphore after reading...\n");
        V(sem_id);
        printf("Client: Sending signal back to server...\n");
        kill(getppid(), SIGUSR1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <shm_id> <sem_id>\n", argv[0]);
        return 1;
    }
    printf("Client: Connecting to shared memory...\n");
    shm_id = atoi(argv[1]);
    sem_id = atoi(argv[2]);

    shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    printf("Client: Connecting to semaphore...\n");
    signal(SIGUSR1, signal_handler);

    while (1) {
        pause();
    }
    shmdt(shm_ptr);
    return 0;
}