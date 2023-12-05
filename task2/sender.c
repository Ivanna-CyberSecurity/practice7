#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define SHARED_MEMORY_SIZE 1024

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int shared_memory_id;
void *shared_memory_ptr;
pid_t child_process_pid;
int semaphore_id;

void custom_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        int sum;
        memcpy(&sum, shared_memory_ptr, sizeof(int));
        printf("Custom Server: Sum: %d\n", sum);
    }
}

void initialize_semaphore(int sem_id, int sem_value) {
    union semun argument;
    argument.val = sem_value;
    if (semctl(sem_id, 0, SETVAL, argument) == -1) {
        perror("semctl");
        exit(1);
    }
}

void wait_semaphore(int sem_id) {
    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op = -1;
    operations[0].sem_flg = 0;
    if (semop(sem_id, operations, 1) == -1) {
        perror("semop P");
        exit(1);
    }
}

void signal_semaphore(int sem_id) {
    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op = 1;
    operations[0].sem_flg = 0;
    if (semop(sem_id, operations, 1) == -1) {
        perror("semop V");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    shared_memory_id = shmget(IPC_PRIVATE, SHARED_MEMORY_SIZE, IPC_CREAT | 0666);
    shared_memory_ptr = shmat(shared_memory_id, NULL, 0);

    printf("Custom Server: Creating shared memory segment...\n");
    if (shared_memory_ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Custom Server: Creating semaphore...\n");
    semaphore_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semaphore_id == -1) {
        perror("semget");
        exit(1);
    }

    printf("Custom Server: Initializing semaphore...\n");
    initialize_semaphore(semaphore_id, 1);

    signal(SIGUSR1, custom_signal_handler);

    printf("Custom Server: Creating child process...\n");
    child_process_pid = fork();

    if (child_process_pid == 0) {
        char shared_memory_id_str[10];
        char semaphore_id_str[10];
        sprintf(shared_memory_id_str, "%d", shared_memory_id);
        sprintf(semaphore_id_str, "%d", semaphore_id);
        execlp("./custom_child", "custom_child", shared_memory_id_str, semaphore_id_str, (char *)NULL);
        perror("execlp");
        exit(1);
    }

    while (1) {
        int n, i, input;

        printf("Enter quantity of nums to sum (0 - exit): ");
        scanf("%d", &n);

        if (n <= 0) {
            break;
        }

        printf("Custom Server: Locking semaphore before writing data...\n");
        wait_semaphore(semaphore_id);

        for (i = 0; i < n; i++) {
            printf("Enter a number: ");
            scanf("%d", &input);
            memcpy((int *)shared_memory_ptr + i, &input, sizeof(int));
        }

        printf("Custom Server: Releasing semaphore after writing...\n");
        signal_semaphore(semaphore_id);

        printf("Custom Server: Sending signal to child process...\n");
        kill(child_process_pid, SIGUSR1);

        printf("Custom Server: Waiting for response from child...\n");
        pause();
    }

    kill(child_process_pid, SIGTERM);
    waitpid(child_process_pid, NULL, 0);

    shmdt(shared_memory_ptr);
    shmctl(shared_memory_id, IPC_RMID, NULL);
    semctl(semaphore_id, 0, IPC_RMID, 0);

    return 0;
}