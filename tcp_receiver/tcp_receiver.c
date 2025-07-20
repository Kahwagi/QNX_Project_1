/**
 * * @file tcp_receiver.c
 * * @brief TCP receiver application that listens for sensor data, decrypts it using AES-128-CBC,
 * *        verifies its integrity using CMAC, and prints the sensor data.
 * * * This application is designed to run on a server that receives encrypted sensor data over TCP.
 * * * It uses OpenSSL for cryptographic operations and can be compiled on both Windows and QNX.    
 * * * @note This code requires OpenSSL library to be installed and linked during compilation.
 * * * @note Ensure to define the AES key and IV in aes_key.h before compiling.
 * * * @note The TCP port can be configured in tcp_conf.h.  
 * * * @author mohamed.elkahwagy@seitech-solutions.com
 * * * @date 07-July-2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/cmac.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else // QNX
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include "config.h"
#include "aes_key.h"


// Structure to hold sensor data
typedef struct
{
    float temperature; // in °C
    float speed;       // in km/h
    float latitude;    // GPS latitude
    float longitude;   // GPS longitude
} sensor_data_t;

/* 
 * Function to decrypt AES-128-CBC encrypted data
 * ciphertext: The encrypted data to decrypt
 * ciphertext_len: Length of the encrypted data
 * key: The AES key used for decryption
 * iv: The initialization vector used for decryption
 * plaintext: Buffer to store the decrypted data
 * Returns: Length of the decrypted plaintext, -1 on error
 * Note: The plaintext buffer should be large enough to hold the decrypted data 
 */
int decrypt(unsigned char *ciphertext, int ciphertext_len,
            unsigned char *key, unsigned char *iv,
            unsigned char *plaintext)
{
    if (!ciphertext || !key || !iv || !plaintext)
    {
        fprintf(stderr, "Invalid parameters for decryption\n");
        return -1; // Invalid parameters
    }

    int len = 0, plaintext_len = 0;

    // Create and initialize the context for decryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    // Initialize decryption operation with AES-128-CBC
    EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);

    // Decrypt the ciphertext (may be called multiple times for large data)
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;

    // Finalize decryption (handles padding)
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;

    // Clean up
    EVP_CIPHER_CTX_free(ctx);

    // Return total length of decrypted plaintext
    return plaintext_len;
}

/*
 * Function to verify the CMAC of received ciphertext
 * key: AES key for CMAC
 * ciphertext: Data to verify
 * ciphertext_len: Length of ciphertext
 * received_cmac: CMAC received from sender
 * Returns: 1 if valid, 0 if invalid, -1 on error
 */
int verify_cmac(const unsigned char *key,
                const unsigned char *ciphertext, size_t ciphertext_len,
                const unsigned char *received_cmac)
{
    unsigned char expected_cmac[CMAC_SIZE];
    size_t cmac_len = 0;

    CMAC_CTX *ctx = CMAC_CTX_new();
    if (!ctx)
        return -1;

    // Initialize CMAC context with AES-128-CBC
    if (!CMAC_Init(ctx, key, 16, EVP_aes_128_cbc(), NULL))
    {
        CMAC_CTX_free(ctx);
        return -1;
    }

    // Update CMAC with ciphertext
    if (!CMAC_Update(ctx, ciphertext, ciphertext_len))
    {
        CMAC_CTX_free(ctx);
        return -1;
    }

    // Finalize and get the computed CMAC
    if (!CMAC_Final(ctx, expected_cmac, &cmac_len))
    {
        CMAC_CTX_free(ctx);
        return -1;
    }

    CMAC_CTX_free(ctx);

    // Compare computed CMAC with received CMAC
    return (cmac_len == CMAC_SIZE && memcmp(expected_cmac, received_cmac, CMAC_SIZE) == 0) ? 1 : 0;
}

int main()
{
    int server_fd, client_fd, bytes_received;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    sensor_data_t sensor_data; // Structure to hold received sensor data
    unsigned char *buffer = NULL;

#ifdef _WIN32
    // Initialize Winsock on Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        return EXIT_FAILURE;
    }
#endif

    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Setup server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
#ifdef _WIN32
        closesocket(server_fd);
#else
        close(server_fd);
#endif
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        closesocket(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Receiver started. Listening on port %d...\n", TCP_PORT);

    // Main server loop: accept and process incoming connections
    while (1)
    {
        puts("------------------------------------------------------------------------------------------");

        // Accept a new client connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1)
        {
            perror("accept");
            continue;
        }

        // Allocate buffer for incoming data
        buffer = (unsigned char *)malloc(BUFFER_SIZE);
        if (buffer == NULL)
        {
            perror("malloc failed for buffer");
            #ifdef _WIN32
            closesocket(client_fd);
            #else
            close(client_fd);
            #endif
            continue;
        }

        // Clear buffer before receiving data
        memset(buffer, 0, BUFFER_SIZE);

        // Receive data from client
        bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        printf("Received %d bytes from client\n", bytes_received);

        if (bytes_received > 0)
        {
            #if ENABLE_DECRYPTION
            // Check if received data is large enough to contain CMAC
            if (bytes_received < CMAC_SIZE)
            {
                fprintf(stderr, "Received data too short for CMAC\n");
                #ifdef _WIN32
                closesocket(client_fd);
                #else
                close(client_fd);
                #endif
                free(buffer);
                continue;
            }

            // Extract ciphertext and CMAC from received buffer
            int ciphertext_len = bytes_received - CMAC_SIZE;
            unsigned char recvd_cmac[CMAC_SIZE] = {0};

            unsigned char *ciphertext = (unsigned char *)malloc(ciphertext_len);
            if (NULL == ciphertext)
            {
                perror("malloc failed for ciphertext buffer");
                closesocket(client_fd);
                free(buffer);
                continue;
            }

            memcpy(ciphertext, buffer, ciphertext_len);
            memcpy(recvd_cmac, buffer + ciphertext_len, CMAC_SIZE);
            free(buffer);

            // Verify CMAC to ensure data integrity and authenticity
            printf("Verifying CMAC...   \n");
            int verified = verify_cmac(aes_key, ciphertext, ciphertext_len, recvd_cmac);
            if (verified != 1)
            {
                fprintf(stderr, /* RED */"\033[1;31mCMAC verification failed! Possible tampering attempt!!!\033[0m\n"/* RESET */);
                #ifdef _WIN32
                closesocket(client_fd);
                #else
                close(client_fd);
                #endif
                free(ciphertext);
                continue;
            }
            printf("\033[1;32mCMAC verification successful!!\033[0m\n");

            /* Decrypt the ciphertext */
            unsigned char *decrypted = (unsigned char *)calloc(sizeof(sensor_data_t), sizeof(unsigned char));
            if (decrypted == NULL)
            {
                perror("calloc failed for decrypted data buffer");
                free(ciphertext);
                closesocket(client_fd);
                continue;
            }

            int decrypted_len = decrypt(ciphertext, ciphertext_len,
                                        (unsigned char *)aes_key, (unsigned char *)aes_iv, decrypted);
            free(ciphertext);

            if (decrypted_len < 0)
            {
                fprintf(stderr, "Decryption failed!!\n");
                free(decrypted);
                closesocket(client_fd);
                continue;
            }
            // Check if the decrypted data size matches the expected structure size.
            // This is important to detect protocol errors or incorrect padding after decryption.
            if (decrypted_len != sizeof(sensor_data_t))
            {
                fprintf(stderr, "Decrypted data size mismatch: expected %zu, got %d\n", sizeof(sensor_data_t), decrypted_len);
                free(decrypted);
                closesocket(client_fd);
                continue;
            }

            // Copy decrypted data into sensor_data structure
            memcpy(&sensor_data, (sensor_data_t *)decrypted, decrypted_len);
            free(decrypted);
            #else
            // If not decrypting, expect raw sensor_data_t structure
            if (bytes_received != sizeof(sensor_data_t))
            {
                fprintf(stderr, "Received data size mismatch: expected %zu, got %d\n", sizeof(sensor_data_t), bytes_received);
                closesocket(client_fd);
                free(buffer);
                continue;
            }
            memcpy(&sensor_data, buffer, sizeof(sensor_data_t));
            #endif
            // Print decrypted sensor data
            printf("Decrypted Sensor Data:: ");
            printf("Temperature: %.1f°C, Speed: %.1f km/h, GPS: (%.4f, %.4f)\n",
                   sensor_data.temperature, sensor_data.speed,
                   sensor_data.latitude, sensor_data.longitude);
        }
        else
        {
            perror("recv");
            fprintf(stderr, "recv returned %d bytes (error or connection closed)\n", bytes_received);
            free(buffer);
        }

        #ifdef _WIN32
        closesocket(client_fd); // close connection after one message
        #else
        close(client_fd); // close connection after one message
        #endif
    }

    #ifdef _WIN32
    closesocket(server_fd);
    #else
    close(server_fd);
    #endif

    #ifdef _WIN32
    WSACleanup();
    #endif

    return 0;
}
