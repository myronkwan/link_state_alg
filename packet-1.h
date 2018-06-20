
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define SIZE 10
typedef struct {
	char checksum;//make checksum at the front of the packet, easier.
	int sequenceAck;
	int length;
	int fin;
} HEADER;
typedef struct {
	HEADER header;
	int data[3];
} PACKET;

