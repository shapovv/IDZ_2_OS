#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#define TREASURE_FOUND 1

typedef struct {
  int *area_status;
  int treasure_found;
} shared_data;

void pirate_group(int group_id, shared_data *data, sem_t *semaphore, int area_count) {
    while (1) {
        sem_wait(semaphore);

        if (data->treasure_found) {
            sem_post(semaphore);
            break;
        }

        int area_to_search = -1;
        for (int i = 0; i < area_count; ++i) {
            if (data->area_status[i] == 0) {
                area_to_search = i;
                data->area_status[i] = -1;
                break;
            }
        }

        sem_post(semaphore);

        if (area_to_search == -1) {
            printf("Пиратская группа %d: все участки проверены.\n", group_id);
            break;
        }

        printf("Пиратская группа %d: ищет на участке %d.\n", group_id, area_to_search);
        sleep(1); // Имитация поиска клада

        if (rand() % area_count == area_to_search) {
            printf("Пиратская группа %d: нашла клад на участке %d!\n", group_id, area_to_search);

            sem_wait(semaphore);
            data->area_status[area_to_search] = TREASURE_FOUND;
            data->treasure_found = 1;
            sem_post(semaphore);

            break;
        } else {
            printf("Пиратская группа %d: клад на участке %d не найден.\n", group_id, area_to_search);
            data->area_status[area_to_search] = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Используйте: %s <AREA_COUNT> <PIRATE_GROUPS>\n", argv[0]);
        exit(1);
    }

    int AREA_COUNT = atoi(argv[1]);
    int PIRATE_GROUPS = atoi(argv[2]);

    srand(time(NULL));

    shared_data *data = mmap(NULL,
                             sizeof(shared_data) + sizeof(int) * AREA_COUNT,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS,
                             -1,
                             0);
    data->area_status = (int *) (data + 1);

    for (int i = 0; i < AREA_COUNT; ++i) {
        data->area_status[i] = 0;
    }
    data->treasure_found = 0;

    sem_t *semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem_init(semaphore, 1, 1) == -1) {
        perror("sem_init");
        exit(1);
    }

    pid_t pid;
    for (int i = 0; i < PIRATE_GROUPS; ++i) {
        pid = fork();

        if (pid == 0) {
            pirate_group(i + 1, data, semaphore, AREA_COUNT);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
    }

    for (int i = 0; i < PIRATE_GROUPS; ++i) {
        wait(NULL);
    }

    printf("Джон Сильвер: клад найден!\n");

    sem_destroy(semaphore);
    munmap(semaphore, sizeof(sem_t));
    munmap(data, sizeof(shared_data) + sizeof(int) * AREA_COUNT);

    return 0;
}

