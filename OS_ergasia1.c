#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define LINE 102
// keys
#define CHILD (key_t) 123
#define PARENT (key_t) 124
#define MUTEX (key_t) 125
#define FINISH (key_t) 126
#define SHM (key_t) 4321

// My Shared Memory
struct shared_mem_st {
    char buff[LINE];
    unsigned int line;
};

// Returns semaphore value
int sem_Val(sem_t *sem) {
    int val;
    sem_getvalue(sem, &val);
    return val;
}

int main (int argc, char* argv[]) {
    void *shared_memory = (void *)0;
    struct shared_mem_st *shared_stuff;
    int shmid; // Shared memory ID
    sem_t *sem1; // Semaphore for child processes
    sem_t *sem2; // Semaphore for parent process
    sem_t *sem3; // Semaphore for mutual exclusion
    sem_t *sem4; // Semaphore for exit
    FILE *X = fopen(argv[1], "r");
    int K = atoi(argv[2]);
    int N = atoi(argv[3]);
    pid_t pid;
    int linecnt = 1; // Line counter
    char l;
    int i;

    if (X == NULL) {
        fprintf(stderr, "Could not open file\n");
        exit(EXIT_FAILURE);
    }

    // Count the lines in file X
    for (l = getc(X); l != EOF; l = getc(X)) {
        if (l == '\n') {
            linecnt++;
        }
    }
    fclose(X);

    // Make shared memory segment
    shmid = shmget(SHM, sizeof(struct shared_mem_st), 0666 | IPC_CREAT);

    if (shmid == -1) {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }
    
    // Attach memory segment
    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        fprintf(stderr, "shmat failed\n");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    
    // Create and initialise named semaphores
    sem1 = sem_open("child", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
    if (errno == EEXIST ) {
	    //printf("semaphore appears to exist already!\n");
		sem1 = sem_open("child", 0);
	}
    sem2 = sem_open("parent", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
    if (errno == EEXIST ) {
		//printf("semaphore appears to exist already!\n");
		sem2 = sem_open("parent", 0);
	}
    sem3 = sem_open("mutex", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);
    if (errno == EEXIST ) {
		//printf("semaphore appears to exist already!\n");
		sem3 = sem_open("mutex", 0);
	}
    sem4 = sem_open("finish", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
    if (errno == EEXIST ) {
		//printf("semaphore appears to exist already!\n");
		sem4 = sem_open("finish", 0);
	}

    //printf("Shared memory segment with id %d attached at %p\n", shmid, shared_memory);
    
    shared_stuff = (struct shared_mem_st *)shared_memory;
    
    // Make K child-processes
    for (i = 0; i < K; i++) {
        pid = fork();
        if (pid == 0) break;
    }
    if (pid < 0) {
        fprintf(stderr, "Process creation fail\n");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // Child process
        //printf("Entered child process\n");
        srand(time(NULL) + getpid());
        clock_t tik, tok;
        double cpu_time_used;
        double avg_time = 0.0;
        int ask, take; // No of line
        char* out_line; // Text to print
        for (i = 0; i < N; i++) {
            sem_wait(sem3);
            tik = clock();
            ask = rand() % linecnt + 1; // Random number in [1, linecnt]
            shared_stuff->line = ask;
            printf("\nChild process with id (%d) asks for line: %d\n", getpid(), ask);
            sem_post(sem2);
            sem_wait(sem1);
            out_line = shared_stuff->buff;
            take = shared_stuff->line;
            sem_post(sem3);
            printf("(%d) line %d: %s", getpid(), take, out_line);
            tok = clock();
            avg_time += ((double)(tok - tik)/ CLOCKS_PER_SEC);
            
        }
        cpu_time_used = (avg_time / (double)N);
        
        printf("--(%d)-- Average time per transaction: %f Clocks/Sec\n", getpid(), cpu_time_used);
        sem_post(sem4);
        if (sem_Val(sem4) == K) { // Only executed at last process
            sem_post(sem2);
        }
        exit(EXIT_SUCCESS);
    } 
    else {
        // Parent process
        //printf("Entering parent process. . .\n");
        while(1) {
            sem_wait(sem2);
            if (sem_Val(sem4) == K) {
                wait(NULL);
                //printf("\nParent process waited!\n");
                break;
            } else {
                int no_asked = shared_stuff->line;
                char buff[LINE];
                int reply = 0;
                X = fopen(argv[1], "r");
                while(fgets(buff, LINE + 1, X) != NULL) {
                    if (++reply == no_asked) {
                        buff[LINE - 2] = '\n';
                        buff[LINE - 1] = '\0';
                        strncpy(shared_stuff->buff, buff, LINE);
                        printf("Parent process delivers line: %d\n", reply);
                        sem_post(sem1);
                        break;
                    }
                }
                fclose(X);
            }
        }
    }

    //printf("All child processes finished!\n");

    // Close and unlink the semaphores
    if (shmdt(shared_memory) == -1) {
        fprintf(stderr, "shmdt failed");
        exit(EXIT_FAILURE);
    }
    sem_close(sem1);
    if (sem_unlink("child") == -1) {
        fprintf(stderr, "Destruction of semaphore failed\n");
        exit(EXIT_FAILURE);
    }
    sem_close(sem2);
    if (sem_unlink("parent") == -1) {
        fprintf(stderr, "Destruction of semaphore failed\n");
        exit(EXIT_FAILURE);
    }
    sem_close(sem3);
    if (sem_unlink("mutex") == -1) {
        fprintf(stderr, "Destruction of semaphore failed\n");
        exit(EXIT_FAILURE);
    }
    sem_close(sem4);
    if (sem_unlink("finish") == -1) {
        fprintf(stderr, "Destruction of semaphore failed\n");
        exit(EXIT_FAILURE);
    }
    
    //printf("Semaphores removed\n");
    
    // Remove shared memory segment
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "shmctl failed\n");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
