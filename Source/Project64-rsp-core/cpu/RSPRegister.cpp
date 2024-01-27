#include "RspTypes.h"
#include <string.h>

// RSP registers
UWORD32 RSP_GPR[32], RSP_Flags[4];
UDWORD RSP_ACCUM[8];
RSPVector RSP_Vect[32];
uint16_t Reciprocals[512];
uint16_t InverseSquareRoots[512];
uint16_t RcpResult, RcpIn;
bool RcpHigh;

RSPFlag VCOL(RSP_Flags[0].UB[0]), VCOH(RSP_Flags[0].UB[1]);
RSPFlag VCCL(RSP_Flags[1].UB[0]), VCCH(RSP_Flags[1].UB[1]);
RSPFlag VCE(RSP_Flags[2].UB[0]);

const char * GPR_Strings[32] = {
    "R0",
    "AT",
    "V0",
    "V1",
    "A0",
    "A1",
    "A2",
    "A3",
    "T0",
    "T1",
    "T2",
    "T3",
    "T4",
    "T5",
    "T6",
    "T7",
    "S0",
    "S1",
    "S2",
    "S3",
    "S4",
    "S5",
    "S6",
    "S7",
    "T8",
    "T9",
    "K0",
    "K1",
    "GP",
    "SP",
    "S8",
    "RA",
};

void InitilizeRSPRegisters(void)
{
    memset(RSP_GPR, 0, sizeof(RSP_GPR));
    for (size_t i = 0, n = sizeof(RSP_Vect) / sizeof(RSP_Vect[0]); i < n; i++)
    {
        RSP_Vect[i] = RSPVector();
    }
    Reciprocals[0] = 0xFFFF;
    for (uint16_t i = 1; i < 512; i++)
    {
        Reciprocals[i] = uint16_t((((1ull << 34) / (uint64_t)(i + 512)) + 1) >> 8);
    }

    for (uint16_t i = 0; i < 512; i++)
    {
        uint64_t a = (i + 512) >> ((i % 2 == 1) ? 1 : 0);
        uint64_t b = 1 << 17;
        while (a * (b + 1) * (b + 1) < (uint64_t(1) << 44))
        {
            b++;
        }
        InverseSquareRoots[i] = uint16_t(b >> 1);
    }

    RcpResult = 0;
    RcpIn = 0;
    RcpHigh = false;
}

int64_t AccumulatorGet(uint8_t el)
{
    return (((int64_t)RSP_ACCUM[el].HW[3]) << 32) | (((int64_t)RSP_ACCUM[el].UHW[2]) << 16) | RSP_ACCUM[el].UHW[1];
}

void AccumulatorSet(uint8_t el, int64_t Accumulator)
{
    RSP_ACCUM[el].HW[3] = (int16_t)(Accumulator >> 32);
    RSP_ACCUM[el].HW[2] = (int16_t)(Accumulator >> 16);
    RSP_ACCUM[el].HW[1] = (int16_t)(Accumulator);
}

uint16_t AccumulatorSaturate(uint8_t el, bool High)
{
    if (RSP_ACCUM[el].HW[3] < 0)
    {
        if (RSP_ACCUM[el].UHW[3] != 0xFFFF || RSP_ACCUM[el].HW[2] >= 0)
        {
            return High ? 0x8000 : 0x0000;
        }
        else
        {
            return RSP_ACCUM[el].UHW[High ? 2 : 1];
        }
    }
    if (RSP_ACCUM[el].UHW[3] != 0 || RSP_ACCUM[el].HW[2] < 0)
    {
        return High ? 0x7fff : 0xffff;
    }
    return RSP_ACCUM[el].UHW[High ? 2 : 1];
}
