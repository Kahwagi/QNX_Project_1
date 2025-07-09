/**
 * @file sensor_client.c
 * @brief Sensor Simulator Client
 * @details
 *  This client simulates a sensor that generates random sensor data
 *  and sends it to a server process using QNX message passing.
 *  The sensor data includes temperature, speed, and GPS coordinates.
 *  The client connects to the server using a name service and sends data periodically every 1 second.
 *  The server is expected to be running and registered with the name SENSOR_NAME.
 *  The client will retry connecting to the server for up to 60 seconds.
 *  If the connection fails after 60 seconds, it will exit with an error.
 * @note
   The client uses the QNX message passing API to send structured sensor data defined in sensor_def.h.
* @author mohamed.elkahwagy@seitech-solutions.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include "sensor_def.h"

void generate_sensor_data(sensor_data_t *data)
{
    data->temperature = (rand() % 1000) / 10.0f;            // 0.0 to 100.0 °C
    data->speed = (rand() % 2000) / 10.0f;                  // 0.0 to 200.0 km/h
    data->latitude = 30.0 + ((rand() % 10000) / 10000.0f);  // 30.000 to 30.999
    data->longitude = 31.0 + ((rand() % 10000) / 10000.0f); // 31.000 to 31.999
}

int main()
{
    int coid;
    message_t msg;
    sensor_data_t data;
    srand(time(NULL));
    int timeout = 0;

    // Attempt to connect to the server using name_open
    // The server should be running and registered with the name SENSOR_NAME
    coid = name_open(SENSOR_NAME, 0);
    while (coid == -1 && timeout < 60)
    {
        timeout++;
        perror("name_open failed");
        printf("Waiting for sensor server to start...\n");
        sleep(1); // wait for 1 second before retrying
        coid = name_open(SENSOR_NAME, 0);
    }
    if (coid == -1)
    {
        fprintf(stderr, "Failed to connect to sensor server after 60 seconds.\n");
        exit(EXIT_FAILURE);
    }
    // Successfully connected to the server
    printf("Connected to sensor server with coid: %d\n", coid);

    // Start generating and sending sensor data
    printf("Sensor simulator started. Sending data every 1s...\n");

    while (1)
    {
        generate_sensor_data(&data);
        msg.type = SENSOR_MSG_TYPE;
        msg.data = data;

        if (MsgSend(coid, &msg, sizeof(msg), NULL, 0) == -1)
        {
            perror("MsgSend failed");
        }
        else
        {
            printf("Sent data: Temp=%.1f°C, Speed=%.1fkm/h, GPS=(%.4f, %.4f)\n",
                   data.temperature, data.speed, data.latitude, data.longitude);
        }

        sleep(1); // wait for 1 second
    }

    name_close(coid);
    return 0;
}
