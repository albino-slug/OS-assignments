#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include "a2_helper.h"
#include <sys/types.h>

// USEFUL IN PART 1
char name1[] = "THREAD1";
char name2[] = "THREAD2";
char name3[] = "THREAD3";
char name4[] = "THREAD4";
char name5[] = "THREAD5";

// USEFUL IN PART 2 / INTRA-PROCESS SYNC.
int t23_has_started    = 0;
int t24_has_terminated = 0;
pthread_mutex_t lock_t23;
pthread_mutex_t lock_t24;
pthread_cond_t  cond_t23_start;
pthread_cond_t  cond_t24_end;

// USEFUL IN PART 3 / THREAD BARRIER
pthread_cond_t cond_enter;
pthread_mutex_t lock_enter;
int threads_running = 0;
int sem_id3;
int sem_id4;
int sem_id5;
int t14_wants_inside = 0;

// USEFUL IN PART 4 / INTER-PROCESS SYNC.
int sem_id1;
int sem_id2;

// DECREMENT A SEMAPHORE
void P (int semId, short unsigned int semNr, short int how_much){
    struct sembuf op = {semNr, how_much, 0};

    semop(semId, &op, 1);
}

// INCREMENT A SEMAPHORE
void V (int semId, short unsigned int semNr, short int how_much){
    struct sembuf op = {semNr, how_much, 0};

    semop(semId, &op, 1);
}

// THE THREAD ROUTINE FOR THREADS IN PROCESS P2
void run_thread_in_P2(void *thread_name){
        char *tmp = (char *)thread_name;
        int id = atoi(tmp + 6);

        // MAKING SURE THREAD T2.2 DOES NOT START BEFORE THREAD T6.1 HAS TERMINATED
        if (id == 2){
                P(sem_id1, 0, -1);              // WAITING FOR T6.1 TO TERMINATE
                info(BEGIN, 2, id);
                info(END, 2, id);
                V(sem_id2, 0, 1);               // SIGNALING T6.3 THAT IT MAY START
        }

        // MAKING SURE THAT T2.3 WAITS FOR THE TERMINATION OF T2.4, BEFORE TERMINATING ITSELF
        else if (id == 3 && !t23_has_started){
                info(BEGIN, 2, id);

                // SIGNAL T2.4 THAT T2.3 HAS STARTED RUNNING
                t23_has_started = 1;
                if (pthread_cond_signal(&cond_t23_start)  != 0) {
                        printf("Cannot signal the condition waiters");
                        exit(0);
                }

                // LOCK THREAD T2.3
                if (pthread_mutex_lock(&lock_t23) != 0) {
                        printf("Cannot take the lock");
                        exit(0);
                }

                // WAIT FOR T2.4 TO TERMINATE
                while(!t24_has_terminated){
                        if (pthread_cond_wait(&cond_t24_end, &lock_t23) != 0) {
                                printf("Cannot wait for condition");
                                exit(0);
                        }
                }

                // UNLOCK T2.3
                if (pthread_mutex_unlock(&lock_t23) != 0) {
                        printf("Cannot release the lock");
                        exit(0);
                }

                info(END, 2, id);
        }

        // MAKING SURE T2.4 BEGINGS AFTER T2.3 HAS BEGUN
        else if (id == 4){
                // LOCK THREAD T2.4
                if (pthread_mutex_lock(&lock_t24) != 0) {
                        printf("Cannot take the lock");
                        exit(0);
                }

                // WAIT FOR T2.3 TO START
                while(!t23_has_started){
                        if (pthread_cond_wait(&cond_t23_start, &lock_t24) != 0) {
                                printf("Cannot wait for condition");
                                exit(0);
                        }
                }

                // UNLOCK T2.4
                if (pthread_mutex_unlock(&lock_t24) != 0) {
                        printf("Cannot release the lock");
                        exit(0);
                }

                info(BEGIN, 2, id);
                info(END, 2, id);

                // SIGNAL T2.3 THAT T2.4 HAS TERMINATED
                t24_has_terminated = 1;
                if (pthread_cond_signal(&cond_t24_end)  != 0) {
                        printf("Cannot signal the condition waiters");
                        exit(0);
                }
        }

        // NO CONDITION IMPOSED ON THE OTHER THREADS
        else {
                info(BEGIN, 2, id);
                info(END, 2, id);
        }
}

// THE THREAD ROUTINE FOR THREAD T14 IN PROCESS P4
void run_thread_P4_T14 (void *thread_name){
        char *tmp = (char *)thread_name;
        int id = atoi(tmp + 6);

        info(BEGIN, 4, id);

        P(sem_id3, 0, -4);      // WAITING FOR THE CRITICAL ZONE TO FILL UP WITH ANOTHER 4 THREADS

        info(END, 4, id);

        V(sem_id4, 0, 43);      // RE-INCREMENTING THE SEMAPHORE SO THE REST OF THE THREADS CAN PASS
}

// THE THREAD ROUTINE FOR THREADS IN PROCESS P4
void run_thread_in_P4(void *thread_name){
        char *tmp = (char *)thread_name;
        int id = atoi(tmp + 6);

        // LOCK THREAD
        if (pthread_mutex_lock(&lock_enter) != 0) {
                printf("Cannot take the lock");
                exit(0);
        }

        // CHECK WHETHER IT CAN RUN, I.E. LESS THAN 4 OTHERS ALREADY RUNNING
        while(threads_running >= 4){
                if (pthread_cond_wait(&cond_enter, &lock_enter) != 0) {
                        printf("Cannot wait for condition");
                        exit(0);
                }
        }

        // INCREMENT THE NUMBER OF THREADS IN CRITICAL ZONE AND SIGNAL ALL THE OTHERS
        threads_running ++;
        V(sem_id3, 0, 1);       // AFTER BEING INCREMENTED 4 TIMES, T14 CAN BEGIN
        if (pthread_cond_broadcast(&cond_enter)  != 0) {
                printf("Cannot signal the condition waiters");
                exit(0);
        }

        // UNLOCK THREAD
        if (pthread_mutex_unlock(&lock_enter) != 0) {
                printf("Cannot take the lock");
                exit(0);
        }

        // BEGIN RUNNING OF THREAD
        info(BEGIN, 4, id);
        printf("The number of threads in the limited area is: %d\n", threads_running);

        P(sem_id4, 0, -1);      // WAITS TO BE INCREMENTED AFTER T14 IS TERMINATED
        // LOCK THREAD AGAIN
        if (pthread_mutex_lock(&lock_enter) != 0) {
                printf("Cannot take the lock");
                exit(0);
        }

        // TERMINATE RUNNING OF THREAD
        info(END, 4, id);

        // DECREMENT THE NUMBER OF THREADS IN CRITICAL ZONE AND SIGNAL ALL THE OTHERS
        threads_running --;
        if (pthread_cond_broadcast(&cond_enter)  != 0) {
                printf("Cannot signal the condition waiters");
                exit(0);
        }

        // UNLOCK THREAD
        if (pthread_mutex_unlock(&lock_enter) != 0) {
                printf("Cannot take the lock");
                exit(0);
        }
}

// THE THREAD ROUTINE FOR THREADS IN PROCESS P6
void run_thread_in_P6(void *thread_name){
        char *tmp = (char *)thread_name;
        int id = atoi(tmp + 6);

        // MAKING SURE THREAD T2.2 IS SIGNALED AFTER T6.1 HAS ENDED
        if (id == 1){
                info(BEGIN, 6, id);
                info(END, 6, id);

                V(sem_id1, 0, 1);           // SIGNALING T2.2 THAT IT MAY START
        }

        // MAKING SURE THAT THREAD T6.3 WAITS FOR THE TERMINATION OF T2.2 BEFORE STARTING
        else if (id == 3){
                P(sem_id2, 0, -1);           // WAITING FOR T2.2 TO START

                info(BEGIN, 6, id);
                info(END, 6, id);
        }

        // NO CONDITION IMPOSED ON THE OTHER THREADS
        else {
                info(BEGIN, 6, id);
                info(END, 6, id);
        }
}

// SOLVES PART 2 OF THE ASSIGNMENT, NAMELY CREATING 5 SYNC. THREADS INSIDE PROCESS P2
void create_sync_threads_in_P2(){
        pthread_t t1, t2, t3, t4, t5;
        int *state = (int *)malloc(sizeof(int) * 7);

        // CREATING LOCKS AND COND. VARIABLES FOR MUTUAL EXCLUSION
        if (pthread_mutex_init(&lock_t23, NULL) != 0) {
                printf("Cannot initialize the lock");
                exit(0);
        }
        if (pthread_cond_init(&cond_t23_start, NULL) != 0) {
                printf("Cannot initialize the condition variable");
                exit(0);
        }
        if (pthread_mutex_init(&lock_t24, NULL) != 0) {
                printf("Cannot initialize the lock");
                exit(0);
        }
        if (pthread_cond_init(&cond_t24_end, NULL) != 0) {
                printf("Cannot initialize the condition variable");
                exit(0);
        }

        // THREAD CREATION

        // THREAD 1
        if (pthread_create(&t1, NULL, (void* (*) (void*))run_thread_in_P2, name1) != 0) {
                printf("Error creating thread 1\n");
                exit(0);
        }
        // THREAD 2
        if (pthread_create(&t2, NULL, (void* (*) (void*))run_thread_in_P2, name2) != 0) {
                printf("Error creating thread 2\n");
                exit(0);
        }
        // THREAD 3
        if (pthread_create(&t3, NULL, (void* (*) (void*))run_thread_in_P2, name3) == 0) {
                // THREAD 4
                if (pthread_create(&t4, NULL, (void* (*) (void*))run_thread_in_P2, name4) != 0) {
                        printf("Error creating thread 4\n");
                        exit(0);
                }
        }
        else {
                printf("Error creating thread 3\n");
                exit(0);
        }
        // THREAD 5
        if (pthread_create(&t5, NULL, (void* (*) (void*))run_thread_in_P2, name5) != 0) {
                printf("Error creating thread 5\n");
                exit(0);
        }

        // JOIN ALL THREADS WITH THE MAIN ONE, T2.0
        pthread_join(t1, (void**) &state[1]);
        pthread_join(t2, (void**) &state[2]);
        pthread_join(t3, (void**) &state[3]);
        pthread_join(t4, (void**) &state[4]);
        pthread_join(t5, (void**) &state[5]);

        // DESTROY ALL LOCKS AND CONDITION VARIABLES + FREE ALLOCATED MEMORY
        if (pthread_mutex_destroy(&lock_t23) != 0) {
                printf("Cannot destroy the lock");
                exit(0);
        }
        if (pthread_mutex_destroy(&lock_t24) != 0) {
                printf("Cannot destroy the lock");
                exit(0);
        }
        if (pthread_cond_destroy(&cond_t23_start) != 0) {
                printf("Cannot destroy the condition variable");
                exit(0);
        }
        if (pthread_cond_destroy(&cond_t24_end) != 0) {
                printf("Cannot destroy the condition variable");
                exit(0);
        }

        free(state);
}

// SOLVES PART 3 OF THE ASSIGNMENT, NAMELY CREATING 44 SYNC. THREADS INSIDE PROCESS P4
void create_sync_threads_in_P4(){
        int *state   = (int *)malloc(sizeof(int) * 45);
        char *buff   = (char *)malloc(sizeof(char) * 2);
        char **name  = (char **)malloc(sizeof(char *) * 45);
        pthread_t *t = (pthread_t *)malloc(sizeof(pthread_t) * 45);

        // CREATING THE THREAD IDs
        memset(buff, 0, 3);
        for (int i = 1; i <= 44; i++){
                name[i] = (char *)malloc(sizeof(char) * 9);
                strcpy(name[i], "THREAD");
                if (i < 10){
                        buff[0] = i + '0';
                }
                else {
                        buff[0] = i / 10 + '0';
                        buff[1] = i % 10 + '0';
                }
                strcat(name[i], buff);
        }

        // CREATING LOCK AND COND. VARIABLE FOR MUTUAL EXCLUSION
        if (pthread_mutex_init(&lock_enter, NULL) != 0) {
                printf("Cannot initialize the lock");
                exit(0);
        }
        if (pthread_cond_init(&cond_enter, NULL) != 0) {
                printf("Cannot initialize the condition variable");
                exit(0);
        }

        // CREATING THE 44 THREADS
        for (int i = 1; i <= 44; i++){
                if (i == 14) {
                        if (pthread_create(t + i, NULL, (void* (*) (void*))run_thread_P4_T14, name[i]) != 0) {
                                printf("Error creating thread %d\n", i);
                                exit(0);
                        }
                }
                else {
                        if (pthread_create(t + i, NULL, (void* (*) (void*))run_thread_in_P4, name[i]) != 0) {
                                printf("Error creating thread %d\n", i);
                                exit(0);
                        }
                }
        }

        // SYNCHRONIZE WITH MAIN THREAD, THREAD T4.0
        for (int i = 1; i <= 44; i++){
                pthread_join(t[i], (void**) &state[i]);
        }

        // DESTROY LOCK AND CONDITION VARIABLE
        if (pthread_mutex_destroy(&lock_enter) != 0) {
                printf("Cannot destroy the lock");
                exit(0);
        }
        if (pthread_cond_destroy(&cond_enter) != 0) {
                printf("Cannot destroy the condition variable");
                exit(0);
        }

        // FREEING ALL THE ALLOCATED MEMORY
        // for (int i = 1; i <= 45; i++){
        //         free(name + i);
        // }
        free(t);
        free(name);
        free(buff);
        free(state);
}

// SOLVES PART 4 OF THE ASSIGNMENT, NAMELY CREATING 5 SYNC. THREADS INSIDE PROCESS P6
void create_sync_threads_in_P6(){
        pthread_t t1, t2, t3, t4, t5;
        int *state = (int *)malloc(sizeof(int) * 7);

        // CREATE THE THREADS

        // THREAD 1
        if (pthread_create(&t1, NULL, (void* (*) (void*))run_thread_in_P6, name1) != 0) {
                printf("Error creating thread 1\n");
                exit(0);
        }
        // THREAD 2
        if (pthread_create(&t2, NULL, (void* (*) (void*))run_thread_in_P6, name2) != 0) {
                printf("Error creating thread 2\n");
                exit(0);
        }
        // THREAD 3
        if (pthread_create(&t3, NULL, (void* (*) (void*))run_thread_in_P6, name3) != 0) {
                printf("Error creating thread 3\n");
                exit(0);
        }
        // THREAD 4
        if (pthread_create(&t4, NULL, (void* (*) (void*))run_thread_in_P6, name4) != 0) {
                printf("Error creating thread 4\n");
                exit(0);
        }
        // THREAD 5
        if (pthread_create(&t5, NULL, (void* (*) (void*))run_thread_in_P6, name5) != 0) {
                printf("Error creating thread 5\n");
                exit(0);
        }

        // JOIN ALL THREADS WITH MAIN THREAD, T6.0
        pthread_join(t1, (void**) &state[1]);
        pthread_join(t2, (void**) &state[2]);
        pthread_join(t3, (void**) &state[3]);
        pthread_join(t4, (void**) &state[4]);
        pthread_join(t5, (void**) &state[5]);

        free(state);
}

// SOLVES PART 1 OF THE ASSIGNMENT, NAMELY CREATING A TREE OF PROCESSES
void create_process_tree(){
        int *child_pid = (int *)malloc(sizeof(int) * 10);
        memset(child_pid, -1, 10);

        child_pid[2] = fork();                          // P2 IS BORN
        if (child_pid[2] == 0){
                info(BEGIN, 2, 0);

                create_sync_threads_in_P2();            // P2 CREATES 5 SYNCHRONIZED THREADS

                info(END, 2, 0);
                exit(0);
        }                                               // P2 DIES

        child_pid[3] = fork();                          // P3 IS BORN
        if (child_pid[3] == 0){
                info(BEGIN, 3, 0);
                info(END, 3, 0);
                exit(0);
        }                                               // P3 DIES

        child_pid[4] = fork();                          // P4 IS BORN
        if (child_pid[4] == 0){
                info(BEGIN, 4, 0);

                create_sync_threads_in_P4();            // P4 CREATES 44 SYNCHRONIZED THREADS

                child_pid[6] = fork();                  // P6 IS BORN
                if (child_pid[6] == 0){
                        info(BEGIN, 6, 0);

                        create_sync_threads_in_P6();

                        child_pid[7] = fork();          // P7 IS BORN
                        if (child_pid[7] == 0){
                                info(BEGIN, 7, 0);
                                info(END, 7, 0);
                                exit(0);
                        }                               // P7 DIES
                        waitpid(child_pid[7], NULL, 0); // P6 WAITS FOR TERMINATION OF P7

                        info(END, 6, 0);
                        exit(0);
                }                                       // P6 DIES

                child_pid[8] = fork();                  // P8 IS BORN
                if (child_pid[8] == 0){
                        info(BEGIN, 8, 0);
                        info(END, 8, 0);
                        exit(0);
                }                                       // P8 DIES
                waitpid(child_pid[6], NULL, 0);         // P4 WAITS FOR TERMINATION OF P6
                waitpid(child_pid[8], NULL, 0);         // P4 WAITS FOR TERMINATION OF P8

                info(END, 4, 0);
                exit(0);
        }                                               // P4 DIES

        child_pid[5] = fork();                          // P5 IS BORN
        if (child_pid[5] == 0){
                info(BEGIN, 5, 0);
                info(END, 5, 0);
                exit(0);
        }                                               // P5 DIES

        // P1 WAITS FOR TERMINATION OF P2, P3, P4, P5
        waitpid(child_pid[2], NULL, 0);
        waitpid(child_pid[3], NULL, 0);
        waitpid(child_pid[4], NULL, 0);
        waitpid(child_pid[5], NULL, 0);

        free(child_pid);
}

int main(){
        // INITIALIZING TESTER
        init();

        // CREATING NEEDED SEMAPHORE SETS
        sem_id1 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id1 < 0) {
                perror("Error creating the semaphore set");
                exit(0);
        }
        sem_id2 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id2 < 0) {
                perror("Error creating the semaphore set");
                exit(0);
        }
        sem_id3 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id3 < 0) {
                perror("Error creating the semaphore set");
                exit(0);
        }
        sem_id4 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id4 < 0) {
                perror("Error creating the semaphore set");
                exit(0);
        }
        sem_id5 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id5 < 0) {
                perror("Error creating the semaphore set");
                exit(0);
        }

        // INITIALIZING NEEDED SEMAPHORE SETS
        semctl(sem_id1, 0, SETVAL, 0);
        semctl(sem_id2, 0, SETVAL, 0);
        semctl(sem_id3, 0, SETVAL, 0);
        semctl(sem_id4, 0, SETVAL, 0);
        semctl(sem_id5, 0, SETVAL, 0);

        // INFORM ABOUT PROCESS' START
        info(BEGIN, 1, 0);

        // CREATE THE PROCESS HIERARCHY - PART 1 OF ASSIGNMENT
        create_process_tree();

        // FREEING NEEDED SEMAPHORE SETS
        semctl(sem_id1, 0, IPC_RMID, 0);
        semctl(sem_id2, 0, IPC_RMID, 0);
        semctl(sem_id3, 0, IPC_RMID, 0);
        semctl(sem_id4, 0, IPC_RMID, 0);
        semctl(sem_id5, 0, IPC_RMID, 0);

        // INFORM ABOUT PROCESS' TERMINATION
        info(END, 1, 0);
        return 0;
}
