#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define TREASURE_FOUND 1

// Структура данных для хранения статуса поиска клада
typedef struct {
  int *area_status;
  int treasure_found;
} shared_data;

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
    int shm_id = shmget(IPC_PRIVATE, sizeof(shared_data) + sizeof(int) * AREA_COUNT, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(1);
    }

    shared_data *data = (shared_data *) shmat(shm_id, NULL, 0);
    data->area_status = (int *) (data + 1);

    // Инициализация статуса участков
    for (int i = 0; i < AREA_COUNT; ++i) {
        data->area_status[i] = 0;
    }
    data->treasure_found = 0;

    // Создание семафора для синхронизации доступа к разделяемым данным
    int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    // Установка начального значения семафора
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(1);
    }

    pid_t pid;
    // Создание процессов для пиратских групп
    for (int i = 0; i < PIRATE_GROUPS; ++i) {
        pid = fork();

        if (pid == 0) {
            // Выполнение функции пиратской группы для дочернего процесса
            char shm_id_str[20], sem_id_str[20], group_id_str[20], area_count_str[20];
            snprintf(shm_id_str, sizeof(shm_id_str), "%d", shm_id);
            snprintf(sem_id_str, sizeof(sem_id_str), "%d", sem_id);
            snprintf(group_id_str, sizeof(group_id_str), "%d", i + 1);
            snprintf(area_count_str, sizeof(area_count_str), "%d", AREA_COUNT);
            execl("./pirate_group_program",
                  "./pirate_group_program",
                  shm_id_str,
                  sem_id_str,
                  group_id_str,
                  area_count_str,
                  (char *) NULL);
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

    // Удаление семафора и разделяемой памяти
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl");
        exit(1);
    }

    if (shmdt(data) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    return 0;
}

