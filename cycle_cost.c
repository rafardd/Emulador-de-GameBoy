#include <stdint.h>
int op_cost(uint8_t opcode)
{
    switch (opcode)
    {
    case 0x00:
        return 1;
    case 0x01:
        return 3;
    case 0x02:
        return 2;
    case 0x03:
        return 2;
    case 0x04:
        return 1;
    case 0x05:
        return 1;
    case 0x06:
        return 2;
    case 0x07:
        return 1;
    case 0x08:
        return 5;
    case 0x09:
        return 2;
    case 0x0A:
        return 2;
    case 0x0B:
        return 2;
    case 0x0C:
        return 1;
    case 0x0D:
        return 1;
    case 0x0E:
        return 2;
    case 0x0F:
        return 1;
    default:
        return 1;
    }
}