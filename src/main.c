#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// unsigned int crc24a(const unsigned char *in, int len);

#define POLY 0x1864CFB
#define INIT 0xB704CE
#define N 24072

#include "PHY/CODING/coding_defs.h"

int main() {
	printf("Start\n");
	unsigned char data[N / 8];
	srand(time(NULL));
	// Loop for 1000000 executions
	for (long long i = 0; i < 1000000; ++i) {
		printf("\rIteration: %lld", i);
		// Loop for CRC code
		for (int j = 0; j < N / 8; ++j){
			data[j] = rand() & 0xFF;
		}

		volatile unsigned int crc = crc24a(data, sizeof(data));
		(void)crc; // suppress unused variable warning
	}

	printf("\nEnd\n");
	return 0;
}
