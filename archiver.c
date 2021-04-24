#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termio.h>
#include <unistd.h>


#include "archiver.h"

void pack(char* dir_path, char* archive_path) {
    int archive;      // Дескриптор архива
    int root_dir_len; // Длина адреса директории

    // Проверка на существования пути архива
    if (access(archive_path, F_OK) != -1) {
        // Если уже существует
        printf("Ошибка. %s уже существует.\n", archive_path);
        exit(1);

    }

    // Создание выходного архива
    archive = open(archive_path, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (archive == -1) {
        printf("Ошибка. %s не может быть создан.\n", archive_path);
        exit(1);
    }

    // Запись в архив мета данных
    if (write(archive, META, META_LENGTH) != META_LENGTH) {
        printf("Ошибка. Запись мета данных в %s не возможна.\n", archive_path);
        exit(1);
    }

    // Запись длины названия директории
    root_dir_len = strlen(dir_path);
    if (write(archive, &root_dir_len, sizeof(int)) != sizeof(int)) {
        printf("Ошибка. Невозможна запись длины названия директории в %s.\n", archive_path);
        exit(1);
    }

    // Запаковка данных в архив
    _pack(archive, dir_path);

    // Закрытие итогового архива
    if (close(archive) == -1) {
        printf("Ошибка. Невозможно закрыть %s.\n", archive_path);
        exit(1);
    }
}

void _pack(int archive, char* src_path) {
    // Открытие директории
    DIR *dir = opendir(src_path);

    // Если путь к директории - это путь к файлу
    if (dir == NULL) {
        printf("Ошибка %s - не директория.\n", src_path);

        // Пытаемся закрыть архив
        if (close(archive) == -1) {
            printf("Ошибка. Невозможно закрыть архив.\n");
        }

        exit(1);
    }

    // Упаковываем информацию об архиве
    _pack_info(archive, FOLDER_NAME, src_path);

    // Получаем следующую запись в каталоге
    struct dirent *entry;

    // Пока существует следующий файл в директории
    while ((entry = readdir(dir)) != NULL) {
        // Пропускаем вхождение . и ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Получем путь директории
        char file_path[MAX_PATH_LENGTH] = {0};
        snprintf(file_path, sizeof(file_path), "%s/%s", src_path, entry->d_name);
        _remove_extra_slash(file_path);

        if (entry->d_type == DT_DIR) {
            // Если это папка
            _pack(archive, file_path);
        }
        else {
            // Записываем информацию
            _pack_info(archive, FILE_NAME, file_path);

            // Записываем содержимое
            _pack_content(archive, file_path);

            // Эхо печать в консоль
            printf("Записно в архив %s\n", file_path);
        }
    }

    // Закрываем директорию
    if (closedir(dir) == -1) {
        printf("Ошибка. Невозможно закрыть %s.\n", src_path);
        exit(1);
    }
}

void _pack_info(int fd, path_type p_type, char* path) {

    // Тип файла
    int type = (int) p_type;

    // Флаг ошибки
    int error = 1;

    do {
        // Записываем тип (папка или файл)
        if (write(fd, &type, sizeof(int)) != sizeof(int)) {
            printf("Ошибка. Невозможно записать тип файла в архив\n");
            break;
        }

        // Записываем длину пути
        int file_path_len = strlen(path) + 1;
        if (write(fd, &file_path_len, sizeof(int)) != sizeof(int)) {
        printf("Ошибка. Невозможно записать длину пути в архив.\n");
            break;
        }

        // Записываем путь файла
        if (write(fd, path, file_path_len) != file_path_len) {
            printf("Ошибка. Невозможно записать путь в архив.\n");
            break;
        }

        error = 0;
        } while (0);

        if (error) {
            if (close(fd) == -1) {
                printf("Ошибка. Невозможно закрыть архив.\n");
            }
            exit(1);
        }
    
}

void _pack_content(int fd, char* path) {
    int error = 1;      // Флаг ошибка
    int file;           // Файловый дискриптор
    off_t size;         // Размер получаемого содержимого файла
    int file_size;      // размер содержимого файла
    char content_byte;  // Байт для копирования в архив

    // Открываем файл
    file = open(path, O_RDONLY, S_IRUSR);
    if (file == -1) {
        printf("Ошибка. Невозможно открыть %s для чтения.\n", path);
        close(fd);
        exit(1);
    }

    do {
        // Получаем размер файлв
        size = lseek(file, 0, SEEK_END);
        if (size == -1) {
            printf("Ошибка. Невозможно получить %s длину.\n", path);
            break;
        }
        file_size = (int)(size);

        // Переводим на начало
        if (lseek(file, 0, SEEK_SET) == -1) {
            printf("Ошибка. Невозможно поставить курсор %s в начало.\n", path);
            break;
        }

        // Записываем размер содержимого
        if (write(fd, &file_size, sizeof(int)) != sizeof(int)) {
            printf("Ошибка. Невозможно записать размер %s в архив.\n", path);
            break;
        }

        // Чтение содержимого
        for (int num_bytes = 0; num_bytes < file_size; num_bytes++) {
            if (read(file, &content_byte, 1) != 1) {
                printf("Ошибка. Невозможно прочитать содержимое %s.\n", path);
                break;
            }

            // Записывем содержимое
            if (write(fd, &content_byte, 1) != 1) {
                printf("Ошибка. Невозможно записать модержимое %s в архив.\n", path);
                break;
            }
        }

        error = 0;
    } while (0);

    // Закрываем архив
    if (error && close(fd) == -1) {
        printf("Ошибка. Невозможно закрыть архив.\n");
    }

    // Закрываем исходный файл
    if (close(file) == -1) {
        printf("Ошибка. Невозможно закрыть %s.\n", path);
        exit(1);
    }

    // Выход по ошибке
    if (error) {
        exit(1);
    }
}

void unpack(char* archive_path, char* out_file) {
    int error = 1;            // Флаг ошибка
    int archive, file;        // Файловые дискрипторы
    char meta[META_LENGTH];   // Буфер для мета данных
    int root_dir_len;         // Length of name of root directory for replacing
    int type;                 // тип файла
    char* path = NULL;        // Буфер для пути
    int path_len;             // Длина пути файла
    char content_byte;        // Буфер для чтения данных
    int content_len;          // Буфер для длины считываемых данных
    int read_status;          // Статус считываемого
    int write_status;         // Статус записываемого

    // Пытаемся открыть файл
    archive = open(archive_path, O_RDONLY);
    if (archive == -1) {
        printf("Ошибка. Невозможно открыть %s.\n", archive_path);
        exit(1);
    }

    do {
        // считываем метаданные и сравниваем их
        if (read(archive, &meta, META_LENGTH) != META_LENGTH || strcmp(meta, META) != 0) {
            printf("Ошибка. %s не архив.\n", archive_path);
            break;
        }

        // Считываем длину директории
        if (read(archive, &root_dir_len, sizeof(int)) != sizeof(int)) {
            printf("Ошибка. Невозможно считать длину директории %s.\n", archive_path);
            break;
        }

        error = 0;
    } while (0);

    if (error) {
        close(archive);
        exit(0);
    }

    // Начинаем получать файлы из архива
    while (read(archive, &type, sizeof(int)) == sizeof(int)) {
        error = 1;

        // Считываем длину
        if (read(archive, &path_len, sizeof(int)) != sizeof(int)) {
            printf("Ошибка. Невозможно считать длину пути из %s.\n", archive_path);
            break;
        }

        // Выделием память для пути
        path = (char*)malloc(path_len);
        if (path == NULL) {
            printf("Ошибка. Невозможно выделить память для пути с длинной %i.\n", path_len);
            break;
        }

        // Считываем путь из архива
        if (read(archive, path, path_len) != path_len) {
            printf("Ошибка. Невозможно считать путь из архива %s.\n", archive_path);
            free(path);
            break;
        }

        path = _rename_root(path, root_dir_len, out_file);
        if (path == NULL) {
            printf("Ошибка. Невозможно распокавать архив с новым именем.\n");
            break;
        }

        // Если путь уже существует
        if (access(path, F_OK) != -1) {
            printf("Ошибка. Невозможно распаковать. %s уже существует.\n", path);
            free(path);
            break;
        }

        if ((path_type)type == FILE_NAME) {
            // Получаем длину содержимого файла
            if (read(archive, &content_len, sizeof(int)) != sizeof(int)) {
                printf("Ошибка. Невозможно считать длину содержимого файла из %s.\n", archive_path);
                free(path);
                break;
            }

            // Открытие файла для записи
            file = open(path, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
            if (file == -1) {
                printf("Ошибка. Невозможно создать %s.\n", path);
                free(path);
                break;
            }

            // Запись содержимого
            for (int num_bytes = 0; num_bytes < content_len; num_bytes++) {
                read_status = read(archive, &content_byte, 1);
                write_status = write(file, &content_byte, 1);

                // Отлов ошибок
                if (read_status != 1 || write_status != 1) {
                    printf("Ошибка. Невозможно записать данные из %s.\n", archive_path);

                    if (close(file) == -1) {
                        printf("Ошибка. Невозможно закрыть %s.\n", path);
                    }

                    if (close(archive) == -1) {
                        printf("Ошибка. Невозможно закрыть %s.\n", archive_path);
                    }

                    free(path);

                    exit(1);
                }
            }

            // Эхо печать
            printf("Распаковано %s\n", path);

            // Закрытие файлового дескриптора
            if (close(file) == -1) {
                printf("Ошибка. Невозможно закрыть %s.\n", path);
                free(path);
                break;
            }
        }

        if ((path_type)type == FOLDER_NAME) {
            // Создаем новую директорию
            if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
                printf("Ошибка, невозможно создать %s.\n", path);
                free(path);
                break;
            }
        }

        free(path);

        error = 0;
    }

    if (close(archive) == -1) {
        printf("Ошибка. Невозможно закрыть %s.\n", archive_path);
        exit(0);
    }

    // Exit if error
    if (error) {
        exit(1);
    }

    // New line for pretty printing
    printf("\n");
}



void _remove_extra_slash(char* path) {
    int i = 0;
    int renamed_len = strlen(path);

    while (i < renamed_len) {
        if((path[i] == '/') && (path[i + 1] == '/')) {
            for(int k = i + 1; k < renamed_len; k++) {
                path[k] = path[k + 1];
            }

            renamed_len -= 1;
        }
        else {
            i += 1;
        }
    }
}

    char* _rename_root(char* path, int old_root_len, char* new_root) {

        int trimmed_path_len = strlen(path) - old_root_len;
        int new_root_len = strlen(new_root);

        char* renamed = (char*)malloc(new_root_len + trimmed_path_len + 2);

        if (renamed == NULL) {
            free(path);
            return NULL;
        }

        memcpy(renamed, new_root, new_root_len);
        renamed[new_root_len++] = '/';

        memcpy(renamed + new_root_len, path + old_root_len, trimmed_path_len);
        renamed[new_root_len + trimmed_path_len] = '\0';

        _remove_extra_slash(renamed);

        free(path);

        return renamed;
    }



