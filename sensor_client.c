/**
  This is a sensor simulator app running as a QNX Process
  it simulates different sensor values like temperature, speed, or GPS.
  Periodically generates values (e.g., every 1s).
  Sends data via QNX message passing or shared memory to another process.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include "sensor_def.h"


void generate_sensor_data(sensor_data_t *data) {
    data->temperature = (rand() % 1000) / 10.0f;      // 0.0 to 100.0 °C
    data->speed = (rand() % 2000) / 10.0f;           // 0.0 to 200.0 km/h
    data->latitude = 30.0 + ((rand() % 10000) / 10000.0f);  // 30.000 to 30.999
    data->longitude = 31.0 + ((rand() % 10000) / 10000.0f); // 31.000 to 31.999
}

int main() {
    int coid;
    message_t msg;
    sensor_data_t data;
    srand(time(NULL));

    // Connect to the receiver
    coid = name_open(SENSOR_NAME, 0);
    if (coid == -1) {
        perror("name_open failed");
        exit(EXIT_FAILURE);
    }

    printf("Sensor simulator started. Sending data every 1s...\n");

    while (1) {
        generate_sensor_data(&data);
        msg.type = SENSOR_MSG_TYPE;
        msg.data = data;

        if (MsgSend(coid, &msg, sizeof(msg), NULL, 0) == -1) {
            perror("MsgSend failed");
        } else {
            printf("Sent data: Temp=%.1f°C, Speed=%.1fkm/h, GPS=(%.4f, %.4f)\n",
                data.temperature, data.speed, data.latitude, data.longitude);
        }

        sleep(1); // wait for 1 second
    }

    name_close(coid);
    return 0;
}
