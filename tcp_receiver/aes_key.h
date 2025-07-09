/**
 * * @file aes_key.h
 * * @brief Header file containing the AES key and IV for encryption/decryption.
 * *        This key is used in the sensor application for encrypting data and in the TCP receiver for decrypting it.
 * * * @note Ensure to keep this key secure and do not expose it in public repositories.
 * * * @note The key and IV should be the same in both the sensor and TCP receiver applications
 * * * @author mohamed.elkahwagy@seitech-solutions.com
 */

#ifndef AES_KEY_H
#define AES_KEY_H

static const unsigned char aes_key[16] = {
    0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B, 0x0C,0x0D,0x0E,0x0F
};

static const unsigned char aes_iv[16] = {
    0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B, 0x1C,0x1D,0x1E,0x1F
};

#endif // AES_KEY_H