#ifndef JJ_NET_H
#define JJ_NET_H

#include <stdint.h>

typedef union JJN_Address {
	uint8_t bytes[16];
	uint32_t dwords[4];
	uint64_t qwords[2];
} JJN_Address;

typedef struct JJN_State JJN_State;


#endif