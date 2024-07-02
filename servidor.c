#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_VERNACULAR_NAMES 10
#define MAX_VERNACULAR_NAME_LENGTH 100
#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    char path[1000];
    unsigned char hash[32];
    unsigned int hash_len;
    off_t size;
    char vernacular_names[MAX_VERNACULAR_NAMES][MAX_VERNACULAR_NAME_LENGTH];
    int vernacular_count;
} FileInfoVernacular;

void print_hash(unsigned char hash[], unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

int compare_hash(unsigned char hash1[], unsigned char hash2[], unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        if (hash1[i] != hash2[i]) {
            return 0;
        }
    }
    return 1;
}

void handle_client(int client_socket) {
    unsigned char received_hash[32];
    int bytes_received = recv(client_socket, received_hash, 32, 0);
    if (bytes_received != 32) {
        perror("Error al recibir el hash");
        close(client_socket);
        return;
    }

    // Lista de nombres de archivos binarios a buscar
    const char *file_names[] = {
        "archivo_recibido_192.168.26.14.bin",
        "archivo_recibido_192.168.26.2.bin"
        // Agrega más nombres de archivos según sea necesario
    };
    int num_files = sizeof(file_names) / sizeof(file_names[0]);

    int file_found = 0;
    FILE *input_file = NULL;

    for (int i = 0; i < num_files; ++i) {
        input_file = fopen(file_names[i], "rb");
        if (!input_file) {
            perror("Error al abrir el archivo");
            continue;  // Intentar con el siguiente archivo
        }

        FileInfoVernacular file_info;
        while (fread(&file_info, sizeof(FileInfoVernacular), 1, input_file)) {
            if (compare_hash(file_info.hash, received_hash, file_info.hash_len)) {
                file_found = 1;
                FILE *file_to_send = fopen(file_info.path, "rb");
                if (!file_to_send) {
                    perror("Error al abrir el archivo para enviar");
                    fclose(input_file);
                    close(client_socket);
                    return;
                }

                // Inform the client that the file was found and send the received hash
                send(client_socket, "FOUND", 5, 0);
                send(client_socket, received_hash, 32, 0);

                // Send the file name first
                const char *file_name = strrchr(file_info.path, '/');
                if (file_name == NULL) {
                    file_name = file_info.path; // No slash found, use the whole path
                } else {
                    file_name++; // Skip the '/'
                }
                uint32_t file_name_len = strlen(file_name);
                send(client_socket, &file_name_len, sizeof(file_name_len), 0);
                send(client_socket, file_name, file_name_len, 0);

                // Send the file size
                fseek(file_to_send, 0, SEEK_END);
                long file_size = ftell(file_to_send);
                fseek(file_to_send, 0, SEEK_SET);
                send(client_socket, &file_size, sizeof(file_size), 0);

                // Send the file data
                char buffer[BUFFER_SIZE];
                int bytes_read;
                while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file_to_send)) > 0) {
                    send(client_socket, buffer, bytes_read, 0);
                }

                fclose(file_to_send);
                break;  // Archivo encontrado y enviado
            }
        }

        fclose(input_file);

        if (file_found) {
            break;  // Si se encontró el archivo, salir del bucle
        }
    }

    if (!file_found) {
        // Inform the client that the file was not found and send the received hash
        send(client_socket, "NOTFOUND", 8, 0);
        send(client_socket, received_hash, 32, 0);
    }

    close(client_socket);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error al enlazar el socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) == -1) {
        perror("Error al escuchar en el socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("Error al aceptar la conexión");
            continue;
        }

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}
