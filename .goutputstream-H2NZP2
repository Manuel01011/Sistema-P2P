#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

void print_hash(unsigned char hash[], unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

int main() {
    FILE *input_file = fopen("archivo_recibido_192.168.26.2.bin", "rb");
    if (!input_file) {
        perror("Error al abrir el archivo");
        return 1;
    }

    FileInfoVernacular file_info;
    while (fread(&file_info, sizeof(FileInfoVernacular), 1, input_file)) {
        printf("Ubicación: %s\n", file_info.path);
        printf("SHA-256: ");
        print_hash(file_info.hash, file_info.hash_len);
        printf("Tamaño: %ld bytes\n", file_info.size);
        printf("Nombres vernáculos:\n");
        for (int i = 0; i < file_info.vernacular_count; ++i) {
            printf("- %s\n", file_info.vernacular_names[i]);
        }
        printf("\n");
    }

    fclose(input_file);
    return 0;
}
