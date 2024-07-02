#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_VERNACULAR_NAMES 10
#define MAX_VERNACULAR_NAME_LENGTH 100

typedef struct {
    char path[1000];
    unsigned char hash[32];
    unsigned int hash_len;
    off_t size;
    char vernacular_names[MAX_VERNACULAR_NAMES][MAX_VERNACULAR_NAME_LENGTH];
    int vernacular_count;
} FileInfoVernacular;

typedef struct {
    char path[1000];
    char pattern[100];
    FILE *output_file;
    pthread_mutex_t *file_lock;
} ThreadArg;

typedef struct {
    int new_socket;
} ClientArg;

// Declaración de funciones
void generate_vernacular_names(const char *filename, char vernacular_names[MAX_VERNACULAR_NAMES][MAX_VERNACULAR_NAME_LENGTH], int *vernacular_count);
void calculate_sha256(const char *path, unsigned char output[32], unsigned int *output_len);
void *process_file(void *arg);
void list_files(const char *base_path, const char *pattern, FILE *output_file, pthread_mutex_t *file_lock);
void *start_server(void *arg);
void *handle_client(void *arg);
void request_file(const char *ip_address);

void generate_vernacular_names(const char *filename, char vernacular_names[MAX_VERNACULAR_NAMES][MAX_VERNACULAR_NAME_LENGTH], int *vernacular_count) {
    *vernacular_count = 0;
    int len = strlen(filename);

    // Variaciones simples: agregar prefijos y sufijos
    if (*vernacular_count < MAX_VERNACULAR_NAMES)
        snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "copy_of_%s", filename);
    if (*vernacular_count < MAX_VERNACULAR_NAMES)
        snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "%s_backup", filename);
    if (*vernacular_count < MAX_VERNACULAR_NAMES)
        snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "%s_v2", filename);
    if (*vernacular_count < MAX_VERNACULAR_NAMES)
        snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "new_%s", filename);
    if (*vernacular_count < MAX_VERNACULAR_NAMES)
        snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "%s_old", filename);

    // Variaciones con cambio de caso
    for (int i = 0; i < len && *vernacular_count < MAX_VERNACULAR_NAMES; i++) {
        if (filename[i] >= 'a' && filename[i] <= 'z') {
            char temp[MAX_VERNACULAR_NAME_LENGTH];
            strcpy(temp, filename);
            temp[i] = temp[i] - 'a' + 'A';
            if (*vernacular_count < MAX_VERNACULAR_NAMES)
                snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "%s", temp);
        } else if (filename[i] >= 'A' && filename[i] <= 'Z') {
            char temp[MAX_VERNACULAR_NAME_LENGTH];
            strcpy(temp, filename);
            temp[i] = temp[i] - 'A' + 'a';
            if (*vernacular_count < MAX_VERNACULAR_NAMES)
                snprintf(vernacular_names[(*vernacular_count)++], MAX_VERNACULAR_NAME_LENGTH, "%s", temp);
        }
    }
}

void calculate_sha256(const char *path, unsigned char output[32], unsigned int *output_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("No se pudo abrir el archivo");
        *output_len = 0;
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    long long int hash_value = 0;
    int primo1 = 31;
    int primo2 = 1000000007;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytesRead; i++) {
            hash_value = (hash_value * primo1 + buffer[i]) % primo2;
        }
    }

    fclose(file);

    for (int i = 0; i < 32; i++) {
        output[i] = (hash_value >> (i * 8)) & 0xFF;
    }

    *output_len = 32;
}

void *process_file(void *arg) {
    ThreadArg *threadArg = (ThreadArg *)arg;
    struct stat st;
    if (stat(threadArg->path, &st) == 0) {
        FileInfoVernacular file_info;
        strncpy(file_info.path, threadArg->path, sizeof(file_info.path));
        calculate_sha256(threadArg->path, file_info.hash, &file_info.hash_len);
        file_info.size = st.st_size;

        char *filename = strrchr(threadArg->path, '/');
        if (filename) filename++; else filename = threadArg->path;

        generate_vernacular_names(filename, file_info.vernacular_names, &file_info.vernacular_count);

        pthread_mutex_lock(threadArg->file_lock);
        fwrite(&file_info, sizeof(FileInfoVernacular), 1, threadArg->output_file);
        pthread_mutex_unlock(threadArg->file_lock);
    }
    return NULL;
}

void list_files(const char *base_path, const char *pattern, FILE *output_file, pthread_mutex_t *file_lock) {
    struct dirent *dp;
    DIR *dir = opendir(base_path);
    if (!dir) {
        perror("Error al abrir el directorio");
        return;
    }

    pthread_t threads[100];
    ThreadArg threadArgs[100];
    int threadCount = 0;

    while ((dp = readdir(dir)) != NULL) {
        if (threadCount >= 100) {
            for (int i = 0; i < threadCount; i++) {
                pthread_join(threads[i], NULL);
            }
            threadCount = 0;
        }

        char path[1000];
        snprintf(path, sizeof(path), "%s/%s", base_path, dp->d_name);

        if (dp->d_type == DT_DIR) {
            if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
                list_files(path, pattern, output_file, file_lock);
            }
        } else {
            if (strstr(dp->d_name, pattern)) {
                strncpy(threadArgs[threadCount].path, path, sizeof(threadArgs[threadCount].path));
                strncpy(threadArgs[threadCount].pattern, pattern, sizeof(threadArgs[threadCount].pattern));
                threadArgs[threadCount].output_file = output_file;
                threadArgs[threadCount].file_lock = file_lock;
                pthread_create(&threads[threadCount], NULL, process_file, &threadArgs[threadCount]);
                threadCount++;
            }
        }
    }

    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
    }

    closedir(dir);
}

void *start_server(void *arg) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en el bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Error en el listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Error en el accept");
            continue;
        }

        pthread_t thread_id;
        ClientArg *clientArg = malloc(sizeof(ClientArg));
        clientArg->new_socket = new_socket;
        pthread_create(&thread_id, NULL, handle_client, clientArg);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return NULL;
}

void *handle_client(void *arg) {
    ClientArg *clientArg = (ClientArg *)arg;
    int new_socket = clientArg->new_socket;
    free(clientArg);
    char buffer[BUFFER_SIZE] = {0};
    read(new_socket, buffer, BUFFER_SIZE);
    printf("Solicitud recibida: %s\n", buffer);

    FILE *file = fopen("archivo_binario.bin", "rb");
    if (file == NULL) {
        perror("No se pudo abrir el archivo");
        close(new_socket);
        return NULL;
    }

    while (!feof(file)) {
        size_t bytesRead = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytesRead > 0) {
            send(new_socket, buffer, bytesRead, 0);
        }
    }

    fclose(file);
    close(new_socket);
    return NULL;
}

void request_file(const char *ip_address) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error al crear el socket\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        printf("Dirección no válida/ Dirección no soportada\n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Error en la conexión\n");
        return;
    }

    char *request = "Enviar archivo";
    send(sock, request, strlen(request), 0);
    printf("Solicitud enviada a %s\n", ip_address);

    char filename[256];
    snprintf(filename, sizeof(filename), "archivo_recibido_%s.bin", ip_address);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("No se pudo abrir el archivo");
        close(sock);
        return;
    }

    int bytesRead;
    while ((bytesRead = read(sock, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    printf("Archivo recibido de %s\n", ip_address);
    fclose(file);
    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <directory> <pattern>\n", argv[0]);
        return 1;
    }
    const char *dir = argv[1];
    const char *pattern = argv[2];

    FILE *output_file = fopen("archivo_binario.bin", "wb");
    if (!output_file) {
        perror("Error al abrir el archivo");
        return 1;
    }

    pthread_mutex_t file_lock;
    pthread_mutex_init(&file_lock, NULL);

    list_files(dir, pattern, output_file, &file_lock);

    pthread_mutex_destroy(&file_lock);
    fclose(output_file);

    // Iniciar el servidor en un hilo separado
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, start_server, NULL);

    sleep(1); // Esperar a que el servidor se inicie

    // Lista de IPs de otras instancias del programa
    const char *ips[] = {
        "192.168.26.14", // Para pruebas locales
        "192.168.26.2" // Agrega más IPs aquí para pruebas en red
    };
    int num_ips = sizeof(ips) / sizeof(ips[0]);

    // Solicitar archivos binarios de otras IPs
    for (int i = 0; i < num_ips; i++) {
        request_file(ips[i]);
    }

    // Esperar a que el servidor termine (nunca termina en este caso)
    pthread_join(server_thread, NULL);
    return 0;
}
