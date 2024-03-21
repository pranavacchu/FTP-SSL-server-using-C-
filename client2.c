#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_BUFFER_SIZE 1024

void list_files(SSL *ssl);
void download_file(SSL *ssl, const char *filename);
void upload_file(SSL *ssl, const char *filename);
int authenticate(SSL *ssl);

int main() {
    printf(" ________  ___       ___  _______   ________   _________         _______     \n");
    printf("|\\   ____\\|\\  \\     |\\  \\|\\  ___ \\ |\\   ___  \\|\\___   ___\\      /  ___  \\    \n");
    printf("\\ \\  \\___|\\ \\  \\    \\ \\  \\ \\   __/|\\ \\  \\\\ \\  \\|___ \\  \\_|     /__/|_/  /|   \n");
    printf(" \\ \\  \\    \\ \\  \\    \\ \\  \\ \\  \\_|/_\\ \\  \\\\ \\  \\   \\ \\  \\      |__|//  / /   \n");
    printf("  \\ \\  \\____\\ \\  \\____\\ \\  \\ \\  \\_|\\ \\ \\  \\\\ \\  \\   \\ \\  \\         /  /_/__  \n");
    printf("   \\ \\_______\\ \\_______\\ \\__\\ \\_______\\ \\__\\\\ \\__\\   \\ \\__\\       |\\________\\\n");
    printf("    \\|_______|\\|_______|\\|__|\\|_______|\\|__| \\|__|    \\|__|        \\|_______|\n");
    printf("\n");
    SSL_CTX *ctx;
    SSL_library_init();
    ctx = SSL_CTX_new(SSLv23_client_method());

    if (!ctx) {
        perror("SSL_CTX_new");
        exit(EXIT_FAILURE);
    }

    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Replace with your server's IP address
    server_addr.sin_port = htons(8888);                   // Replace with your server's port

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_socket);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        close(client_socket);
        SSL_free(ssl);
        exit(EXIT_FAILURE);
    }

    if (!authenticate(ssl)) {
        printf("Authentication failed\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Choose an option:\n");
        printf("1. List FTP Files -> LIST\n");
        printf("2. Download File from FTP -> DOWNLOAD <filename>\n");
        printf("3. Upload File to FTP -> UPLOAD <filename>\n");
        printf("4. Exit -> EXIT\n");
        char command[MAX_BUFFER_SIZE];
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "LIST") == 0) {
            list_files(ssl);
        } else if (strncmp(command, "DOWNLOAD", 8) == 0) {
            char *filename = command + 9;
            download_file(ssl, filename);
        } else if (strncmp(command, "UPLOAD", 6) == 0) {
            char *filename = command + 7;
            upload_file(ssl, filename);
        } else if (strcmp(command, "EXIT") == 0) {
            break;
        } else {
            printf("Invalid command\n");
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    SSL_CTX_free(ctx);

    return 0;
}

void list_files(SSL *ssl) {
    SSL_write(ssl, "LIST", strlen("LIST"));
    
    char response[MAX_BUFFER_SIZE];
    int bytes_received = SSL_read(ssl, response, sizeof(response) - 1);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("File list received from server:\n%s\n", response);
    } else {
        printf("Failed to receive file list from server\n");
    }
}
void download_file(SSL *ssl, const char *filename) {
    char command[MAX_BUFFER_SIZE];
    sprintf(command, "DOWNLOAD:%s", filename);
    SSL_write(ssl, command, strlen(command));

    char file_size_data[MAX_BUFFER_SIZE];
    int bytes_received = SSL_read(ssl, file_size_data, sizeof(file_size_data));
    if (bytes_received <= 0) {
        printf("Failed to receive file size from server\n");
        return;
    }
    file_size_data[bytes_received] = '\0';
    long file_size = atol(file_size_data);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File open failed");
        return;
    }

    int total_bytes_received = 0;
    while (total_bytes_received < file_size) {
        char buffer[MAX_BUFFER_SIZE];
        int bytes_to_receive = (file_size - total_bytes_received) > MAX_BUFFER_SIZE ? MAX_BUFFER_SIZE : (file_size - total_bytes_received);
        bytes_received = SSL_read(ssl, buffer, bytes_to_receive);
        if (bytes_received <= 0) {
            printf("Failed to receive file data from server\n");
            fclose(file);
            return;
        }
        fwrite(buffer, 1, bytes_received, file);
        total_bytes_received += bytes_received;
    }

    printf("Download complete\n");
    fclose(file);
}



void upload_file(SSL *ssl, const char *filename) {
    // Open the local file for reading
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        return;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send the UPLOAD command to the server
    SSL_write(ssl, "UPLOAD", strlen("UPLOAD"));

    // Send the filename to the server
    SSL_write(ssl, filename, strlen(filename));

    // Send the file size to the server
    char file_size_data[MAX_BUFFER_SIZE];
    snprintf(file_size_data, sizeof(file_size_data), "%ld", file_size);
    SSL_write(ssl, file_size_data, strlen(file_size_data));

    // Send the file data to the server in chunks
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        // Write the data to the SSL connection
        int bytes_sent = SSL_write(ssl, buffer, bytes_read);
        if (bytes_sent <= 0) {
            printf("Failed to send file data to server\n");
            fclose(file);
            return;
        }
    }

    // Close the file
    fclose(file);

    printf("Upload complete\n");
}


int authenticate(SSL *ssl) {
    char username[MAX_BUFFER_SIZE];
    char password[MAX_BUFFER_SIZE];

    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0';

    SSL_write(ssl, username, strlen(username));
    SSL_write(ssl, password, strlen(password));

    char auth_status[MAX_BUFFER_SIZE];
    SSL_read(ssl, auth_status, sizeof(auth_status));

    if (strcmp(auth_status, "AUTH_SUCCESS") == 0) {
        return 1;
    } else {
        printf("Authentication failed\n");
        return 0;
    }
}

