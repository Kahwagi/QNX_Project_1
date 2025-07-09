/**
 *
 * config.h - configuration header file for TCP receiver application
 * * This file contains configurable parameters such as TCP port, buffer size,
 * * decryption settings, and CMAC size.
 * 
 */

#ifndef CONFIG_H
#define CONFIG_H


#define TCP_PORT 8000   // Port on which the TCP receiver listens for incoming connections
#define BUFFER_SIZE 128 // Size of the buffer for receiving data
#define ENABLE_DECRYPTION 1 // Set to 1 to enable decryption, 0 to disable
#define CMAC_SIZE 16 // AES-128-CBC CMAC size is 16 bytes

#endif // CONFIG_H
