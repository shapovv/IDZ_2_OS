#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#define TREASURE_FOUND 1

void pirate_group(int group_id, int *shared_memory, sem_t *semaphore, int area_count) {
    while (1) {
        // Ожидание семафора для доступа к разделяемым данным
        sem_wait(semaphore);

        // Проверка, найден ли клад
        if (shared_memory[area_count]) {
            // Освобождение семафора и выход из цикла
            sem_post(semaphore);
            break;
        }

        // Выбор участка для поиска
        int area_to_search = -1;
        for (int i = 0; i < area_count; ++i) {
            if (shared_memory[i] == 0) {
                area_to_search = i;
                // Обозначение участка как проверяемого
                shared_memory[i] = -1;
                break;
            }
        }

        // Освобождение семафора после работы с разделяемыми данными
        sem_post(semaphore);

        // Если все участки проверены, завершаем работу группы
        if (area_to_search == -1) {
            printf("Пиратская группа %d: все участки проверены.\n", group_id);
            break;
        }

        printf("Пиратская группа %d: ищет на участке %d.\n", group_id, area_to_search);
        sleep(1); // Имитация поиска клада

        // Если клад найден на участке
        if (rand() % area_count == area_to_search) {
            printf("Пиратская группа %d: нашла клад на участке %d!\n", group_id, area_to_search);

            // Обновление статуса участка и общего статуса нахождения клада
            sem_wait(semaphore);
            shared_memory[area_to_search] = TREASURE_FOUND;
            shared_memory[area_count] = 1;
            sem_post(semaphore);

            break;
        } else {
            printf("Пиратская группа %d: клад на участке %d не найден.\n", group_id, area_to_search);
            // Обновление статуса участка как проверенного
            shared_memory[area_to_search] = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Используйте: %s <AREA_COUNT>\n", argv[0]);
        exit(1);
    }

    int AREA_COUNT = atoi(argv[1]);
    srand(time(NULL));

    // Открытие разделяемой памяти для доступа к данным
    int shm_fd = shm_open("pirate_data", O_RDWR, 0644);
    int *shared_memory = mmap(NULL, sizeof(int) * (AREA_COUNT + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    // Открытие семафора для синхронизации доступа к разделяемым данным
    sem_t *semaphore = sem_open("pirate_semaphore", 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Выполнение функции пиратской группы
    pirate_group(getpid(), shared_memory, semaphore, AREA_COUNT);

    // Закрытие семафора и разделяемой памяти
    sem_close(semaphore);
    munmap(shared_memory, sizeof(int) * (AREA_COUNT + 1));

    return 0;
}

