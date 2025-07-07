#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <sys/dispatch.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

#include "sensor_def.h"
#include "tcp_conf.h"
#include "aes_key.h"


int aes_encrypt(unsigned char *plaintext, int plaintext_len,
                unsigned char *key, unsigned char *iv,
                unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, ciphertext_len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
    ciphertext_len = len;

    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

// Send the data to a remote TCP server as plaintext
int send_over_tcp(sensor_data_t *data)
{
    int sockfd;
    struct sockaddr_in server_addr;
    unsigned char *plaintext = NULL;
    unsigned char *ciphertext = NULL;

    // Allocate memory for plaintext
    int plaintext_len = sizeof(sensor_data_t);

    plaintext = (unsigned char *)calloc(plaintext_len, sizeof(unsigned char));
    if (plaintext == NULL)
    {
        perror("calloc failed for plaintext");
        return -1;
    }

    // Copy sensor data into plaintext buffer
    memcpy(plaintext, data, plaintext_len);


    ciphertext = (unsigned char *)malloc(plaintext_len + AES_BLOCK_SIZE);
    if (ciphertext == NULL)
    {
        perror("malloc failed for ciphertext");
        free(plaintext);
        return -1;
    }
    memset(ciphertext, 0, sizeof(ciphertext)); // Clear the ciphertext buffer

    int encrypted_len = aes_encrypt(plaintext, plaintext_len,
                                    (unsigned char *)aes_key, (unsigned char *)aes_iv, ciphertext);

    free(plaintext); // Free plaintext after encryption is done

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(REMOTE_IP);

    // Connect to remote server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        close(sockfd);
        return -1;
    }

    // Send the data
    if (send(sockfd, ciphertext, encrypted_len, 0) == -1)
    {
        perror("send");
        close(sockfd);
        return -1;
    }

    free(ciphertext); // Free ciphertext after sending

    printf("Encrypted and sent %d bytes of data over to TCP receiver\n", encrypted_len);

    // Close the socket
    if (close(sockfd) == -1)
    {
        perror("close");
        return -1;
    }
    return 0;
}

int main()
{
    name_attach_t *attach;
    message_t msg;
    int rcvid;

    attach = name_attach(NULL, SENSOR_NAME, 0);
    if (attach == NULL)
    {
        perror("name_attach failed");
        exit(EXIT_FAILURE);
    }

    printf("Sensor receiver started. Waiting for messages...\n");

    while (1)
    {
        rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1)
        {
            perror("MsgReceive failed");
            continue;
        }

        if (rcvid == 0)
        {
            // PULSE, ignore
            continue;
        }

        if (msg.type == SENSOR_MSG_TYPE)
        {
            // Print received sensor data
            printf("Received: Temp=%.1f°C, Speed=%.1fkm/h, GPS=(%.4f, %.4f)\n",
                   msg.data.temperature, msg.data.speed,
                   msg.data.latitude, msg.data.longitude);

            // Send acknowledgment back to sender
            MsgReply(rcvid, 0, NULL, 0);

            // Forward to TCP server
            if (send_over_tcp(&msg.data) != 0)
            {
                fprintf(stderr, "Failed to send data over TCP\n");
            }
        }
        else
        {
            /*unknown message, ignore*/
            continue;
        }
    }

    name_detach(attach, 0);
    return 0;
}
