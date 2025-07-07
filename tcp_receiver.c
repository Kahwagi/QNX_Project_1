#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

#include "tcp_conf.h"
#include "aes_key.h"

#define BUFFER_SIZE 128

typedef struct
{
    float temperature; // in °C
    float speed;       // in km/h
    float latitude;    // GPS lat
    float longitude;   // GPS long
} sensor_data_t;

/* Function to decrypt AES-128-CBC encrypted data
ciphertext: The encrypted data to decrypt
ciphertext_len: Length of the encrypted data
key: The AES key used for decryption
iv: The initialization vector used for decryption
plaintext: Buffer to store the decrypted data
Returns: Length of the decrypted plaintext
         -1 on error
Note: The plaintext buffer should be large enough to hold the decrypted data */
int decrypt(unsigned char *ciphertext, int ciphertext_len,
            unsigned char *key, unsigned char *iv,
            unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, plaintext_len = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;

    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int bytes_received;
    unsigned char *buffer = NULL;

    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Setup address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Receiver started. Listening on port %d...\n", TCP_PORT);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1)
        {
            perror("accept");
            continue;
        }

        // Read and print data
        buffer = (unsigned char *)malloc(BUFFER_SIZE);
        if (buffer == NULL)
        {
            perror("malloc failed for buffer");
            close(client_fd);
            continue;
        }
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0)
        {
            unsigned char *decrypted = (unsigned char *)calloc(sizeof(sensor_data_t), sizeof(unsigned char));
            if (decrypted == NULL)
            {
                perror("calloc failed for decrypted");
                close(client_fd);
                continue;
            }

            int decrypted_len = decrypt(buffer, bytes_received,
                                        (unsigned char *)aes_key, (unsigned char *)aes_iv, decrypted);

            if (decrypted_len < 0)
            {
                fprintf(stderr, "Decryption failed\n");
                free(decrypted);
                close(client_fd);
                continue;
            }

            if (decrypted_len != sizeof(sensor_data_t))
            {
                fprintf(stderr, "Decrypted data size mismatch: expected %zu, got %d\n", sizeof(sensor_data_t), decrypted_len);
                free(decrypted);
                close(client_fd);
                continue;
            }

            sensor_data_t *sensor_data = (sensor_data_t *)decrypted;

            // Print decrypted sensor data
            printf("Received %d bytes from client\n", bytes_received);
            printf("Decrypted Sensor Data:: ");
            printf("Temperature: %.1f°C, Speed: %.1f km/h, GPS: (%.4f, %.4f)\n",
                   sensor_data->temperature, sensor_data->speed,
                   sensor_data->latitude, sensor_data->longitude);
        }
        else
        {
            perror("recv");
        }

        close(client_fd); // close connection after one message
    }

    close(server_fd);
    return 0;
}
