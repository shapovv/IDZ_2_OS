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

// Функция для выполнения действий каждой пиратской группы
void pirate_group(int group_id, shared_data *data, int sem_id, int area_count) {
    struct sembuf sem_op;

    while (1) {
        // Ожидание семафора для доступа к разделяемым данным
        sem_op.sem_num = 0;
        sem_op.sem_op = -1;
        sem_op.sem_flg = 0;
        semop(sem_id, &sem_op, 1);

        // Проверка, найден ли клад
        if (data->treasure_found) {
            // Освобождение семафора и выход из цикла
            sem_op.sem_op = 1;
            semop(sem_id, &sem_op, 1);
            break;
        }

        // Выбор участка для поиска
        int area_to_search = -1;
        for (int i = 0; i < area_count; ++i) {
            if (data->area_status[i] == 0) {
                area_to_search = i;
                // Обозначение участка как проверяемого
                data->area_status[i] = -1;
                break;
            }
        }

        // Освобождение семафора после работы с разделяемыми данными
        sem_op.sem_op = 1;
        semop(sem_id, &sem_op, 1);

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
            sem_op.sem_op = -1;
            semop(sem_id, &sem_op, 1);
            data->area_status[area_to_search] = TREASURE_FOUND;
            data->treasure_found = 1;
            sem_op.sem_op = 1;
            semop(sem_id, &sem_op, 1);

            break;
        } else {
            printf("Пиратская группа %d: клад на участке %d не найден.\n", group_id, area_to_search);
            // Обновление статуса участка как проверенного
            data->area_status[area_to_search] = -1;
        }
    }
}

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
            pirate_group(i + 1, data, sem_id, AREA_COUNT);
            exit(0);
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

