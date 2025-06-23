#
#   Makefile for sensor project
#

# Build option variables
DEBUG = -g 

# Compiler and linker
CC = qcc
LD = qcc

# Target platform
TARGET = -Vgcc_ntox86_64
#TARGET = -Vgcc_ntoarmv7le
#TARGET = -Vgcc_ntoaarch64le

# Compile and link flags
CFLAGS += $(TARGET) $(DEBUG) -Wall 
LDFLAGS+= $(TARGET) $(DEBUG)

# Binaries to build
BINS = sensor_server sensor_client tcp_receiver

# Default target
all: $(BINS)

# Compile rules
sensor_server.o: sensor_server.c sensor_def.h tcp_conf.h
	$(CC) $(CFLAGS) -c sensor_server.c

sensor_client.o: sensor_client.c sensor_def.h
	$(CC) $(CFLAGS) -c sensor_client.c

tcp_receiver.o: tcp_receiver.c tcp_conf.h
	$(CC) $(CFLAGS) -c tcp_receiver.c

# Link rules
sensor_server: sensor_server.o
	$(LD) $(LDFLAGS) -lsocket -o sensor_server sensor_server.o

tcp_receiver: tcp_receiver.o
	$(LD) $(LDFLAGS) -lsocket -o tcp_receiver tcp_receiver.o

sensor_client: sensor_client.o
	$(LD) $(LDFLAGS) -o sensor_client sensor_client.o

# Clean target
clean:
	rm -f *.o $(BINS)
