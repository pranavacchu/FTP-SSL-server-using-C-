

# FTP Server using OpenSSL

This project is a simple FTP server implemented in C, utilizing OpenSSL for secure communication. It allows clients to list files, download files from the server, and upload files to the server securely.This code also has terminal art for better UI.

## Features

- Secure communication using SSL/TLS encryption.
- User authentication with username and password.
- List files available on the server.
- Download files from the server.
- Upload files to the server.
- Terminal Art

## Prerequisites

- C compiler
- OpenSSL library

## Getting Started

1. Clone the repository:

    ```bash
    git clone https://github.com/yourusername/ftp-server.git
    ```

2. Compile the server:

    ```bash
    cd ftp-server
    gcc server.c -o server -lssl -lcrypto -lpthread
    ```

3. Compile the client:

    ```bash
    gcc client.c -o client -lssl -lcrypto
    ```

4. Run the server:

    ```bash
    ./server
    ```

5. Run the client:

    ```bash
    ./client
    ```

6. Follow the prompts to authenticate and perform various FTP operations.

## Usage

- Upon connecting, the client needs to authenticate with a username and password.
- Use the following commands in the client:
  - `LIST`: List files available on the server.
  - `DOWNLOAD <filename>`: Download a file from the server.
  - `UPLOAD <filename>`: Upload a file to the server.
  - `EXIT`: Disconnect from the server.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.


---

Feel free to customize the content and formatting of the README file according to your project's specific details and preferences. Once done, save the content to a file named `README.md` in the root directory of your GitHub repository. This README will serve as a guide for users who visit your project's repository on GitHub.
