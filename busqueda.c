#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void print_hash(unsigned char hash[], unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

int connect_to_server(const char *server_address, unsigned char hash[32]) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error al crear el socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        perror("Dirección inválida");
        close(client_socket);
        return -1;
    }

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al conectar con el servidor");
        close(client_socket);
        return -1;
    }

    send(client_socket, hash, 32, 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(client_socket, response, 5, 0); // Recibir los primeros 5 bytes de la respuesta
    if (bytes_received <= 0) {
        perror("Error al recibir la respuesta del servidor");
        close(client_socket);
        return -1;
    }

    response[bytes_received] = '\0';

    unsigned char received_hash[32];
    if (strcmp(response, "FOUND") == 0) {
        // Recibir el hash recibido del servidor
        bytes_received = recv(client_socket, received_hash, 32, 0);
        if (bytes_received != 32) {
            perror("Error al recibir el hash");
            close(client_socket);
            return -1;
        }

        printf("Archivo encontrado en el servidor %s. Recibiendo archivo...\n", server_address);
        printf("Hash recibido: ");
        print_hash(received_hash, 32);

        // Recibir el nombre del archivo
        uint32_t file_name_len;
        recv(client_socket, &file_name_len, sizeof(file_name_len), 0);
        char file_name[file_name_len + 1];
        recv(client_socket, file_name, file_name_len, 0);
        file_name[file_name_len] = '\0';

        // Recibir el tamaño del archivo
        long file_size;
        recv(client_socket, &file_size, sizeof(file_size), 0);
        FILE *received_file = fopen(file_name, "wb");
        if (!received_file) {
            perror("Error al crear el archivo recibido");
            close(client_socket);
            return -1;
        }

        // Recibir el archivo
        char buffer[BUFFER_SIZE];
        long total_bytes_received = 0;
        while (total_bytes_received < file_size) {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received < 0) {
                perror("Error al recibir el archivo");
                fclose(received_file);
                close(client_socket);
                return -1;
            }
            fwrite(buffer, 1, bytes_received, received_file);
            total_bytes_received += bytes_received;
        }

        fclose(received_file);
        printf("Archivo recibido y guardado como '%s'\n", file_name);
        close(client_socket);
        return 0;  // Archivo encontrado y recibido
    } else if (strcmp(response, "NOTFOUND") == 0) {
        // Recibir el hash recibido del servidor
        bytes_received = recv(client_socket, received_hash, 32, 0);
        if (bytes_received != 32) {
            perror("Error al recibir el hash");
            close(client_socket);
            return -1;
        }

        printf("Archivo no encontrado en el servidor %s.\n", server_address);
        printf("Hash recibido: ");
        print_hash(received_hash, 32);
    } else {
        printf("Respuesta desconocida del servidor: %s\n", response);
    }

    close(client_socket);
    return 1;  // Archivo no encontrado en este servidor
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <hash SHA-256>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned char hash[32];
    for (int i = 0; i < 32; ++i) {
        sscanf(&argv[1][2 * i], "%2hhx", &hash[i]);
    }

    // Lista de direcciones de servidores
    const char *server_addresses[] = {
        "192.168.26.2"
    };
    int num_servers = sizeof(server_addresses) / sizeof(server_addresses[0]);

    for (int i = 0; i < num_servers; ++i) {
        int result = connect_to_server(server_addresses[i], hash);
        if (result == 0) {
            // Archivo encontrado y recibido
            break;
        }
    }

    return 0;
}
