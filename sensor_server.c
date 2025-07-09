/**
 * @file sensor_server.c
 * @brief Sensor Server 
 * @details
 *  This server listens for sensor data messages from clients using QNX message passing.
 *  It receives structured sensor data, encrypts it using AES-128-CBC, 
 *  and generates a CMAC for integrity verification.
 *  The encrypted data along with the CMAC is then sent over TCP to a remote server.
 * * @note
 *  The server uses the QNX message passing API to receive structured sensor data defined in sensor_def.h.
 *  It uses OpenSSL for AES encryption and CMAC generation. 
 * * @author mohamed.elkahwagy@seitech-solutions.com
 */
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
#include <openssl/cmac.h>

#include "sensor_def.h"
#include "tcp_conf.h"
#include "aes_key.h"

/* Function to generate CMAC for the given data
key: The AES key used for CMAC generation    
data: The data to generate CMAC for
data_len: Length of the data
cmac_output: Buffer to store the generated CMAC
cmac_len: Pointer to store the length of the generated CMAC */
int generate_cmac(const unsigned char *key,
                  const unsigned char *data, size_t data_len,
                  unsigned char *cmac_output, size_t *cmac_len)
{
    if (!key || !data || !cmac_output || !cmac_len) {
        return -1; // Invalid parameters
    }

    CMAC_CTX *ctx = CMAC_CTX_new();          
    if (!ctx) return -1;
    
    if (!CMAC_Init(ctx, key, 16, EVP_aes_128_cbc(), NULL)) {
        CMAC_CTX_free(ctx);
        return -1;
    }

    if (!CMAC_Update(ctx, data, data_len)) {
        CMAC_CTX_free(ctx);
        return -1;
    }

    if (!CMAC_Final(ctx, cmac_output, cmac_len)) {
        CMAC_CTX_free(ctx);
        return -1;
    }

    CMAC_CTX_free(ctx);
    return 0;
}


/*  Function: aes_encrypt   
 * 
 *  Encrypts the sensor data using AES-128-CBC   
 *  plaintext: The data to encrypt   
 *  plaintext_len: Length of the data to encrypt
 *  key: The AES key used for encryption
 *  iv: The initialization vector used for encryption
 *  ciphertext: Buffer to store the encrypted data   
 *  Returns: Length of the encrypted ciphertext
 *           -1 on error 
 */
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

// Encrypts sensor data and returns the length of the ciphertext, or -1 on error
int encrypt_sensor_data(sensor_data_t *data, unsigned char **ciphertext, int *ciphertext_len)
{
    int plaintext_len = sizeof(sensor_data_t);
    unsigned char *plaintext = (unsigned char *)calloc(plaintext_len, sizeof(unsigned char));
    if (!plaintext)
    {
        perror("calloc failed for plaintext");
        return -1;
    }
    memcpy(plaintext, data, plaintext_len);

    *ciphertext = (unsigned char *)malloc(plaintext_len + AES_BLOCK_SIZE);
    if (!*ciphertext)
    {
        perror("malloc failed for ciphertext");
        free(plaintext);
        return -1;
    }
    memset(*ciphertext, 0, plaintext_len + AES_BLOCK_SIZE);

    *ciphertext_len = aes_encrypt(plaintext, plaintext_len,
                                  (unsigned char *)aes_key, (unsigned char *)aes_iv, *ciphertext);

    free(plaintext);

    if (*ciphertext_len <= 0)
    {
        free(*ciphertext);
        *ciphertext = NULL;
        return -1;
    }

    // Generate CMAC for the ciphertext
    unsigned char cmac[EVP_MAX_MD_SIZE];
    size_t cmac_len = 0;

    if (generate_cmac(aes_key, *ciphertext, *ciphertext_len, cmac, &cmac_len) != 0)
    {
        free(*ciphertext);
        *ciphertext = NULL;
        fprintf(stderr, "CMAC generation failed\n");
        return -1;
    }
    
    printf("Generated CMAC for encrypted sensor data: [");
    for (size_t i = 0; i < cmac_len; i++)
    {
        printf("%02x", cmac[i]);
    }   
    printf("]\n");

    // Append CMAC to the end of the ciphertext
    *ciphertext = (unsigned char *)realloc(*ciphertext, *ciphertext_len + cmac_len);
    if (NULL == *ciphertext)
    {
        perror("realloc failed for (ciphertext + cmac)!");
        free(*ciphertext);
        return -1;
    }

    memcpy(*ciphertext + *ciphertext_len, cmac, cmac_len);
    *ciphertext_len += cmac_len;

    return 0;
}

// Sends data over TCP, returns 0 on success, -1 on error
int send_over_tcp(unsigned char *sdata, int data_len)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(REMOTE_IP);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, sdata, data_len, 0) == -1)
    {
        perror("send");
        close(sockfd);
        return -1;
    }

    printf("Encrypted and sent %d bytes of data to TCP receiver\n", data_len);

    if (close(sockfd) == -1)
    {
        perror("close");
        return -1;
    }
    return 0;
}

// Wrapper: Encrypts and sends sensor data over TCP
int encrypt_and_send_over_tcp(sensor_data_t *data)
{
    unsigned char *ciphertext = NULL;
    int ciphertext_len = 0;
    int ret = encrypt_sensor_data(data, &ciphertext, &ciphertext_len);
    if (ret != 0)
        return -1;

    ret = send_over_tcp(ciphertext, ciphertext_len);
    free(ciphertext);
    return ret;
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

    printf("Sensor server started. Waiting for messages...\n");

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
            printf("Sensor data: Temp=%.1f°C, Speed=%.1fkm/h, GPS=(%.4f, %.4f)\n",
                   msg.data.temperature, msg.data.speed,
                   msg.data.latitude, msg.data.longitude);

            // Send acknowledgment back to sender
            MsgReply(rcvid, 0, NULL, 0);

            // Forward to TCP server
            // if (send_over_tcp((unsigned char*)(&msg.data),sizeof(msg.data)) != 0)    /*send raw data*/
            if (encrypt_and_send_over_tcp(&msg.data) != 0)  /*encrypt and send*/
            {
                fprintf(stderr, "Failed to send data over TCP\n");
            }
            puts("------------------------------------------------------------------------------------");

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
