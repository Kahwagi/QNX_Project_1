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

#include "sensor_def.h"
#include "tcp_conf.h"

// Send the data to a remote TCP server as plaintext
int send_over_tcp(sensor_data_t *data)
{
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[256];
    
    // Format the sensor data as a simple string
    snprintf(buffer, sizeof(buffer),
             "Temperature: %.1f°C, Speed: %.1f km/h, GPS: (%.4f, %.4f)\n",
             data->temperature, data->speed, data->latitude, data->longitude);
    
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
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
    {
        perror("send");
        close(sockfd);
        return -1;
    }

    printf(">> Sent over TCP: %s", buffer);

    // Close the socket
    if (close(sockfd) == -1)
    {
        perror("close");
        return -1;
    }
    return 0;
}

int main() {
    name_attach_t *attach;
    message_t msg;
    int rcvid;

    attach = name_attach(NULL, SENSOR_NAME, 0);
    if (attach == NULL) {
        perror("name_attach failed");
        exit(EXIT_FAILURE);
    }

    printf("Sensor receiver started. Waiting for messages...\n");

    while (1) {
        rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive failed");
            continue;
        }

        if (rcvid == 0) {
            // PULSE, ignore
            continue;
        }

        if(msg.type == SENSOR_MSG_TYPE)
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
