#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
struct shared {
    char rubric[5];
    int exam;
    int marked[5];
    bool finishedExam;
    bool finished;
};

void loadRubric(int fd, char* rubric) {
    char c;
    for (int i = 0; i < 5; i++) {
        lseek(fd, 3, SEEK_CUR);
        read(fd, &c, 1);
        rubric[i] = c;
        lseek(fd, 2, SEEK_CUR);
    }
    lseek(fd, 0, SEEK_SET);
}

void updateRubric(int fd, char* rubric) {
    char c;
    for (int i = 0; i < 5; i++) {
        lseek(fd, 3, SEEK_CUR);
        c = rubric[i];
        write(fd, &c, 1);
        lseek(fd, 2, SEEK_CUR);
    }
    lseek(fd, 0, SEEK_SET);
}

void resetRubric(int fd) {
    char start[] = "1, A\n2, B\n3, C\n4, D\n5, E";
    write(fd, start, 24);
    lseek(fd, 0, SEEK_SET);
}

void loadNextExam(int* exam) {
    int count;
    if (*exam == -1) {
        count = 1;
    }
    else {
        count = *exam + 1;
    }
    char stid[5];
    bool loaded = false;
    while (!loaded) {
        char filename[9];
        snprintf(filename, 9, "%04d.txt", count);
        int fd = open(filename, O_RDONLY);
        if (fd != -1) {
            read(fd, stid, 4);
            close(fd);
            loaded = true;
        }
        count++;
    }
    *exam = atoi(stid);
}

int main(int argc, char *argv[]) {
    int fd = open("rubric.txt", O_RDWR | O_TRUNC);
    resetRubric(fd);

    if (argc != 2) {
        printf("put arguments\n");
        close(fd);
        return -1;
    }
    
    int numTAs = atoi(argv[1]);
    
    int memoryID;
    do {
        memoryID = shmget(IPC_PRIVATE, sizeof(struct shared), IPC_CREAT | 0666);
    } while (memoryID == -1);

    struct shared* shm = shmat(memoryID, NULL, 0);
    loadRubric(fd, shm->rubric);
    shm->exam = -1;
    loadNextExam(&shm->exam);
    for (int i = 0; i < 5; i++) {
        shm->marked[i] = 0;
    }
    shm->finishedExam = false;
    shm->finished = false;

    srand(time(NULL));

    pid_t pid;

    for (int i = 0; i < numTAs; i++) {
        pid = fork();

        if (pid < 0) {
            printf("Fork Failed\n");
            return 1;
        }
        else if (pid == 0) {
            int TA = i+1;
            while (1) {
                for (int j = 0; j < 5; j++) {
                    printf("TA %d: Checking rubric Q%d\n", TA, j+1);
                    
                    int sleepTime = (rand() % (1000000 - 500000 + 1)) + 500000;
                    usleep(sleepTime);
                    
                    if (rand() % 2) {
                        printf("TA %d: Correcting rubric Q%d\n", TA, j+1);
                        shm->rubric[j] += 1;
                        updateRubric(fd, shm->rubric);
                    }
                    else {
                        printf("TA %d: No correction needed\n", TA);
                    }
                }

                if (shm->finishedExam) shm->finishedExam = false;

                for (int j = 0; j < 5; j++) {
                    if (!(shm->marked[j])) {
                        shm->marked[j] = 1;
                        int sleepTime = (rand() % (2000000 - 1000000 + 1)) + 1000000;
                        usleep(sleepTime);
                        printf("TA %d: Marked Q%d of student %04d\n", TA, j+1, shm->exam);
                        if (j == 4) {
                            if (shm->exam == 9999) {
                                printf("TA %d: Finished final exam. Yay!\n", TA);
                                shm->finished = true;
                                shm->finishedExam = true;
                            }
                            else {
                                printf("TA %d: Finished exam of student %04d, lets move on to the next.\n", TA, shm->exam);
                                loadNextExam(&shm->exam);
                                shm->finishedExam = true;
                                for (int i = 0; i < 5; i++) {
                                    shm->marked[i] = 0;
                                }
                            } 
                        }
                    }
                }

                while (!shm->finishedExam) {}
                if (shm->finished) break;
            }
            printf("TA %d finished.\n", TA);
            exit(EXIT_SUCCESS);
        }
    }

    printf("Waiting for TAs to finish.\n");

    for (int i = 0; i < numTAs; i++) {
        wait(NULL);
    }

    printf("TAs finished.\n");

    shmdt(shm);
    shmctl(memoryID, IPC_RMID, NULL);
    close(fd);
    return 0;
}