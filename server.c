#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#define MAX_BUFFER_SIZE 1024

void *handle_client(void *arg);
void list_files(SSL *ssl);
void send_file(SSL *ssl, const char *filename);
void receive_file(SSL *ssl);

struct Credentials {
    const char *username;
    const char *password;
};

// Example: Add a user with username 'pranav' and password '123'
struct Credentials credentials[] = {
    {"pranav", "123"},
    {"preeti", "456"},
    // Add more users as needed
};

int authenticate(SSL *ssl) {
    char username[1024];
    char password[1024];

    SSL_read(ssl, username, sizeof(username));
    SSL_read(ssl, password, sizeof(password));
    printf("Received Username: %s\n", username);
    printf("Received Password: %s\n", password);

    // Check if the provided username and password match any valid credentials
    for (size_t i = 0; i < sizeof(credentials) / sizeof(credentials[0]); ++i) {
        if (strcmp(username, credentials[i].username) == 0 && strcmp(password, credentials[i].password) == 0) {
            // Authentication successful
            printf("Authentication successful.\n");
            SSL_write(ssl, "AUTH_SUCCESS", sizeof("AUTH_SUCCESS"));
            return 1;
        }
    }
    printf("Authentication failed.\n");
    SSL_write(ssl, "AUTH_FAILURE", sizeof("AUTH_FAILURE"));
    // Authentication failed
    return 0;
}

int main() {
    printf("\n");
    printf("███████╗████████╗██████╗ \n");
    printf("██╔════╝╚══██╔══╝██╔══██╗\n");
    printf("█████╗     ██║   ██████╔╝\n");
    printf("██╔══╝     ██║   ██╔═══╝ \n");
    printf("██║        ██║   ██║     \n");
    printf("╚═╝        ╚═╝   ╚═╝     \n");
    printf("\n");
    printf("███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗ \n");
    printf("██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗\n");
    printf("███████╗█████╗  ██████╔╝██║   ██║█████╗  ██████╔╝\n");
    printf("╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══╝  ██╔══██╗\n");
    printf("███████║███████╗██║  ██║ ╚████╔╝ ███████╗██║  ██║\n");
    printf("╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝\n");
    printf("\n");
    SSL_CTX *ctx;
    SSL_library_init();
    ctx = SSL_CTX_new(SSLv23_server_method());
    

    // Load server certificate and private key
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        perror("Error loading server certificate or private key");
        exit(EXIT_FAILURE);
    }

    if (!ctx) {
        perror("SSL_CTX_new");
        exit(EXIT_FAILURE);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8888);

    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket, 5);

    printf("FTP Server Started.\n");
    printf("Waiting for Client connections\n");

    struct stat st = {0};
    if (stat("SERVER_FILES", &st) == -1) {
        if (mkdir("SERVER_FILES", 0777) == -1) {
            perror("Error creating SERVER_FILES directory");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_addr_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_addr_len);
        printf("Client %s connected.\n", inet_ntoa(client_address.sin_addr));
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_socket);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            close(client_socket);
            SSL_free(ssl);
            continue;
        }

        printf("Client %s connected.\n", inet_ntoa(client_address.sin_addr));

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, ssl);
        pthread_detach(tid);
    }

    close(server_socket);
    SSL_CTX_free(ctx);

    return 0;
}

//handling client connection 
void *handle_client(void *arg) {
    SSL *ssl = (SSL *)arg;
    struct sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);

    getpeername(SSL_get_fd(ssl), (struct sockaddr *)&client_address, &client_addr_len);

    if (authenticate(ssl)) {
        while (1) {
            char data[MAX_BUFFER_SIZE];
            ssize_t bytes_received = SSL_read(ssl, data, sizeof(data));

            if (bytes_received <= 0) {
                printf("Client %s disconnected.\n", inet_ntoa(client_address.sin_addr));
                break;
            } else if (strncmp(data, "LIST", 4) == 0) {
                list_files(ssl);
            } else if (strncmp(data, "DOWNLOAD", 8) == 0) {
                const char *filename = data + 9;
                send_file(ssl, filename);
            } else if (strncmp(data, "UPLOAD", 6) == 0) {
                receive_file(ssl);
                return NULL; // Exit the function after receiving the file
            } else if (strncmp(data, "EXIT", 4) == 0) {
                break; // Exit the loop and close the connection
            } else {
                printf("Invalid command received from client.\n");
            }
            // Add a sleep to avoid busy-waiting
            sleep(1);
        }
    } else {
        printf("Client authentication failed.\n");
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    return NULL;
}


void list_files(SSL *ssl) {
    printf("Listing files...\n");

    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir("SERVER_FILES");
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    char file_list[MAX_BUFFER_SIZE] = "";

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Regular file
            strcat(file_list, entry->d_name);
            strcat(file_list, "\n");
        }
    }

    // Close the directory
    closedir(dir);

    // Send the file list to the client
    SSL_write(ssl, file_list, strlen(file_list));

    printf("File list sent to client\n");
}
void send_file(SSL *ssl, const char *filename) {
    // Construct the full file path
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "SERVER_FILES/%s", filename);

    // Open the file
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("File not found: %s\n", filename);
        SSL_write(ssl, "FILE_NOT_FOUND", sizeof("FILE_NOT_FOUND"));
        return;
    }

    // Determine the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send the file size to the client
    char file_size_data[MAX_BUFFER_SIZE];
    snprintf(file_size_data, sizeof(file_size_data), "%ld", file_size);
    SSL_write(ssl, file_size_data, strlen(file_size_data));

    // Read and send the file contents in chunks
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SSL_write(ssl, buffer, bytes_read);
    }

    // Close the file
    fclose(file);
}



void receive_file(SSL *ssl) {
    char filename[MAX_BUFFER_SIZE];
    ssize_t bytes_received_filename = SSL_read(ssl, filename, sizeof(filename) - 1);

    if (bytes_received_filename <= 0) {
        perror("Error receiving filename");
        return;
    }

    filename[bytes_received_filename] = '\0';

    char file_path[20000];
    snprintf(file_path, sizeof(file_path), "SERVER_FILES/%s", filename);

    FILE *file = fopen(file_path, "wb");
    if (!file) {
        perror("Error opening file for writing");
        return;
    }

    long file_size;
    if (SSL_read(ssl, &file_size, sizeof(file_size)) <= 0) {
        perror("Error receiving file size");
        fclose(file);
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    size_t total_bytes_received = 0;
    ssize_t bytes_received;

    // Continue reading until the entire file is received
    while (total_bytes_received < file_size) {
        bytes_received = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes_received <= 0) {
            perror("Error reading from socket");
            break;
        }
        fwrite(buffer, 1, bytes_received, file);
        total_bytes_received += bytes_received;
    }

    if (total_bytes_received != file_size) {
        perror("Error: Incomplete file received");
    }

    // Close the file
    fclose(file);

    printf("File '%s' received and saved.\n", filename);
}

