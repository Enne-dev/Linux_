#ifndef MYIO3_H
#define MYIO3_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#define LOG1_FILE "LOG1"
#define SHM_TOTAL_SIZE (4096 * 16)
#define HEADER_BYTES 4
#define MAX_DATA_BYTES (SHM_TOTAL_SIZE - HEADER_BYTES)

static int mode = 0;
static int nscanf_total = 0;
static int nscanf_running = 0;
static int nscanf_target = 0;
static int nbytes_total = 0;
static int nbytes_running = 0;

static int shmid = -1;
static void *shm_base = NULL;
static unsigned char *data_buf = NULL;


static void handle_sigint(int signo) {
    int fd = open(LOG1_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0 && shm_base != NULL) {
        write(fd, &nscanf_total, HEADER_BYTES);
        if (nbytes_total > 0) {
            write(fd, data_buf, (size_t)nbytes_total);
        }
        close(fd);
    }
    if (shm_base != NULL && shmid != -1) {
        shmdt(shm_base);
        shmctl(shmid, IPC_RMID, NULL);
        shm_base = NULL;
        shmid = -1;
    }
    execl("./t","t",NULL);
}

static void r_init(void) {
    struct sigaction act;
    act.sa_handler = handle_sigint;
    sigaction(SIGINT, &act, NULL);
    shmid = shmget(IPC_PRIVATE, SHM_TOTAL_SIZE, IPC_CREAT | 0600);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    shm_base = shmat(shmid, NULL, 0);
    if (shm_base == (void *)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    data_buf = (unsigned char*)shm_base + HEADER_BYTES;
    mode = 0;
    nscanf_total = 0;
    nscanf_running = 0;
    nscanf_target = 0;
    nbytes_total = 0;
    nbytes_running = 0;

    int fd = open(LOG1_FILE, O_RDONLY);
    if (fd >= 0) {
        int header = 0;
        ssize_t r = read(fd, &header, HEADER_BYTES);
        if (r == HEADER_BYTES) {
            nscanf_total = header;
            ssize_t got = read(fd, data_buf, MAX_DATA_BYTES);
            if (got < 0) got = 0;
            nbytes_total = (int)(got > MAX_DATA_BYTES ? MAX_DATA_BYTES : got);

            mode = 1;
            nscanf_running = 0;
            nbytes_running = 0;

            printf("복구 모드 활성화. 저장된 입력 개수 = %d\n", nscanf_total);
            printf("복구할 입력 번호를 입력하세요 (1 ~ %d): ", nscanf_total);
            if (scanf("%d", &nscanf_target) != 1) nscanf_target = 0;
            if (nscanf_target < 0) nscanf_target = 0;
            if (nscanf_target > nscanf_total) nscanf_target = nscanf_total;
            printf("%d번째 입력까지 복구 완료.\n", nscanf_target);
        } else {
            mode = 0;
            nscanf_total = 0;
            nbytes_total = 0;
        }
        close(fd);
    } else {
        mode = 0;
    }
}

static int r_scanf(const char *format, void *ptr) {
    if (format == NULL || ptr == NULL) return 0;

    int size;
    int typ;

    if (strchr(format, 'c')) { typ = 1; size = 1; }
    else if (strchr(format, 'f')) { typ = 3; size = 4; }
    else if (strchr(format, 'd')) { typ = 2; size = 4; }
    else return 0;

    if (mode == 1 && nscanf_running < nscanf_target) {
        if ((nbytes_running + size) > nbytes_total) return 0;
        memcpy(ptr, data_buf + nbytes_running, (size_t)size);
        nbytes_running += size;
        nscanf_running += 1;
        if (nscanf_running >= nscanf_target) {
            mode = 0;
            nbytes_total = nbytes_running;
            nbytes_running = 0;
            nscanf_total = nscanf_target;
        }
        return 1;
    }

    if (typ == 1) {
        char tmp;
        scanf(" %c", &tmp);
        *(char*)ptr = tmp;
        data_buf[nbytes_total] = (unsigned char)tmp;
        nbytes_total += 1;
        nscanf_total += 1;
    } else if (typ == 2) {
        int tmp;
        scanf("%d", &tmp);
        *(int*)ptr = tmp;
        memcpy(data_buf + nbytes_total, &tmp, 4);
        nbytes_total += 4;
        nscanf_total += 1;
    } else if (typ == 3) {
        float tmp;
        scanf("%f", &tmp);
        *(float*)ptr = tmp;
        memcpy(data_buf + nbytes_total, &tmp, 4);
        nbytes_total += 4;
        nscanf_total += 1;
    }

    return 1;
}

static int r_printf(const char *format, ...) {
    if (format == NULL) return 0;
    if (mode == 1 && nscanf_running < nscanf_target) return 0;

    va_list ap;
    va_start(ap, format);
    int r = vprintf(format, ap);
    va_end(ap);

    return r;
}

static void r_cleanup(void) {
    unlink(LOG1_FILE);

    if (shm_base != NULL && shmid != -1) {
        shmdt(shm_base);
        shmctl(shmid, IPC_RMID, NULL);
        shm_base = NULL;
        shmid = -1;
    }
}

#endif
