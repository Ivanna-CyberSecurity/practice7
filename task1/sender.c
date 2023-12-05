#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define SHM_SIZE 1024

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int shm_id;
void *shm_ptr;
pid_t child_pid;
int sem_id;

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        int sum;
        memcpy(&sum, shm_ptr, sizeof(int));
        printf("Sum: %d\n", sum);
    }
}

void init_semaphore(int sem_id, int sem_value) {
    union semun argument;
    argument.val = sem_value;
    if (semctl(sem_id, 0, SETVAL, argument) == -1) {
        perror("semctl");
        exit(1);
    }
}

void P(int sem_id) {
    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op = -1;
    operations[0].sem_flg = 0;
    if (semop(sem_id, operations, 1) == -1) {
        perror("semop P");
        exit(1);
    }
}

void V(int sem_id) {
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
    shm_id = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
    shm_ptr = shmat(shm_id, NULL, 0);
    printf("Server: Creating shared memory segment...\n");
    if (shm_ptr == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    printf("Server: Creating semaphore...\n");
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }
    printf("Server: Initializing semaphore...\n");
    init_semaphore(sem_id, 1);

    signal(SIGUSR1, signal_handler);
    printf("Server: Creating child process...\n");
    child_pid = fork();
    if (child_pid == 0) {
        char shm_id_str[10];
        char sem_id_str[10];
        sprintf(shm_id_str, "%d", shm_id);
        sprintf(sem_id_str, "%d", sem_id);
        execlp("./child", "child", shm_id_str, sem_id_str, (char *)NULL);
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
        printf("Server: Locking semaphore before writing data...\n");
        P(sem_id);

        for (i = 0; i < n; i++) {
            printf("Введіть число: ");
            scanf("%d", &input);
            memcpy((int *)shm_ptr + i, &input, sizeof(int));
        }
        printf("Server: Releasing semaphore after writing...\n");
        V(sem_id);
        printf("Server: Sending signal to child process...\n");
        kill(child_pid, SIGUSR1);
        printf("Server: Waiting for response from child...\n");
        pause();
    }

    kill(child_pid, SIGTERM);
    waitpid(child_pid, NULL, 0);
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID, 0);

    return 0;
}