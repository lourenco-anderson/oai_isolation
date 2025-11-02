#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"

#define POLY 0x1864CFB
#define INIT 0xB704CE
#define N 24072

#include "PHY/CODING/coding_defs.h"

void nr_scramble();
void nr_crc();
