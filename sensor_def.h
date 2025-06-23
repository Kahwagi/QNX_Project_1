/**
 * header file containing data types and macros used by sensor application
 * 
 * */
#include <sys/iomsg.h>

 #define SENSOR_NAME "sensor"
 #define SENSOR_MSG_TYPE (_IO_MAX + 100)

typedef struct {
    float temperature; // in °C
    float speed;       // in km/h
    float latitude;    // GPS lat
    float longitude;   // GPS long
} sensor_data_t;

typedef struct {
    uint16_t type;
    sensor_data_t data;
} message_t;
