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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Используйте: %s <AREA_COUNT> <PIRATE_GROUPS>\n", argv[0]);
        exit(1);
    }

    // Получение количества участков и пиратских групп из аргументов
    int AREA_COUNT = atoi(argv[1]);
    int PIRATE_GROUPS = atoi(argv[2]);

    srand(time(NULL));

    // Создание разделяемой памяти для хранения данных
    int shm_fd = shm_open("pirate_data", O_CREAT | O_RDWR, 0644);
    ftruncate(shm_fd, sizeof(int) * (AREA_COUNT + 1));
    int *shared_memory = mmap(NULL, sizeof(int) * (AREA_COUNT + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    // Инициализация статуса участков
    for (int i = 0; i < AREA_COUNT; ++i) {
        shared_memory[i] = 0;
    }
    shared_memory[AREA_COUNT] = 0;

    // Создание семафора для синхронизации доступа к разделяемым данным
    sem_t *semaphore = sem_open("pirate_semaphore", O_CREAT, 0644, 1);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pid_t pid;
    // Создание процессов для пиратских групп
    for (int i = 0; i < PIRATE_GROUPS; ++i) {
        pid = fork();

        if (pid == 0) {
            // Запуск программы пиратской группы для дочернего процесса
            execl("pirate_group_program", "pirate_group_program", argv[1], NULL);
            perror("execl");
            exit(1);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
    }

    // Ожидание завершения всех пиратских групп
    for (int i = 0; i < PIRATE_GROUPS; ++i) {
        wait(NULL);
    }

    printf("Джон Сильвер: клад найден!\n");

    // Закрытие семафора и удаление разделяемой памяти
    sem_close(semaphore);
    sem_unlink("pirate_semaphore");
    munmap(shared_memory, sizeof(int) * (AREA_COUNT + 1));
    shm_unlink("pirate_data");

    return 0;
}

