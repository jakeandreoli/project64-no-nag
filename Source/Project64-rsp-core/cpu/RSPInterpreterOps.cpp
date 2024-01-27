#include "RSPCpu.h"
#include "RSPInterpreterCPU.h"
#include "RSPRegisters.h"
#include "RspLog.h"
#include <Common/StdString.h>
#include <Project64-rsp-core/RSPDebugger.h>
#include <Project64-rsp-core/RSPInfo.h>
#include <Project64-rsp-core/Settings/RspSettings.h>
#include <Project64-rsp-core/cpu/RspClamp.h>
#include <Settings/Settings.h>
#include <algorithm>
#include <float.h>
#include <math.h>

extern UWORD32 Recp, RecpResult, SQroot, SQrootResult;
extern bool AudioHle, GraphicsHle;

uint32_t clz32(uint32_t val)
{
#if defined(__GNUC__)
    return val ? __builtin_clz(val) : 32;
#else
    /* Binary search for the leading one bit.  */
    int cnt = 0;

    if ((val & 0xFFFF0000U) == 0)
    {
        cnt += 16;
        val <<= 16;
    }
    if ((val & 0xFF000000U) == 0)
    {
        cnt += 8;
        val <<= 8;
    }
    if ((val & 0xF0000000U) == 0)
    {
        cnt += 4;
        val <<= 4;
    }
    if ((val & 0xC0000000U) == 0)
    {
        cnt += 2;
        val <<= 2;
    }
    if ((val & 0x80000000U) == 0)
    {
        cnt++;
        val <<= 1;
    }
    if ((val & 0x80000000U) == 0)
    {
        cnt++;
    }
    return cnt;
#endif
}

// Opcode functions

void RSP_Opcode_SPECIAL(void)
{
    RSP_Special[RSPOpC.funct]();
}

void RSP_Opcode_REGIMM(void)
{
    RSP_RegImm[RSPOpC.rt]();
}

void RSP_Opcode_J(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = (RSPOpC.target << 2) & 0xFFC;
}

void RSP_Opcode_JAL(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_GPR[31].UW = (*PrgCount + 8) & 0xFFC;
    RSP_JumpTo = (RSPOpC.target << 2) & 0xFFC;
}

void RSP_Opcode_BEQ(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W == RSP_GPR[RSPOpC.rt].W);
}

void RSP_Opcode_BNE(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W != RSP_GPR[RSPOpC.rt].W);
}

void RSP_Opcode_BLEZ(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W <= 0);
}

void RSP_Opcode_BGTZ(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W > 0);
}

void RSP_Opcode_ADDI(void)
{
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W + (int16_t)RSPOpC.immediate;
}

void RSP_Opcode_ADDIU(void)
{
    RSP_GPR[RSPOpC.rt].UW = RSP_GPR[RSPOpC.rs].UW + (uint32_t)((int16_t)RSPOpC.immediate);
}

void RSP_Opcode_SLTI(void)
{
    RSP_GPR[RSPOpC.rt].W = (RSP_GPR[RSPOpC.rs].W < (int16_t)RSPOpC.immediate) ? 1 : 0;
}

void RSP_Opcode_SLTIU(void)
{
    RSP_GPR[RSPOpC.rt].W = (RSP_GPR[RSPOpC.rs].UW < (uint32_t)(int16_t)RSPOpC.immediate) ? 1 : 0;
}

void RSP_Opcode_ANDI(void)
{
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W & RSPOpC.immediate;
}

void RSP_Opcode_ORI(void)
{
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W | RSPOpC.immediate;
}

void RSP_Opcode_XORI(void)
{
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W ^ RSPOpC.immediate;
}

void RSP_Opcode_LUI(void)
{
    RSP_GPR[RSPOpC.rt].W = RSPOpC.immediate << 16;
}

void RSP_Opcode_COP0(void)
{
    RSP_Cop0[RSPOpC.rs]();
}

void RSP_Opcode_COP2(void)
{
    RSP_Cop2[RSPOpC.rs]();
}

void RSP_Opcode_LB(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    RSP_GPR[RSPOpC.rt].W = *(int8_t *)(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
}

void RSP_Opcode_LH(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x1) != 0)
    {
        RSP_GPR[RSPOpC.rt].UHW[0] = *(uint8_t *)(RSPInfo.DMEM + (((Address + 0) & 0xFFF) ^ 3)) << 8;
        RSP_GPR[RSPOpC.rt].UHW[0] += *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) & 0xFFF) ^ 3)) << 0;
    }
    else
    {
        RSP_GPR[RSPOpC.rt].UHW[0] = *(uint16_t *)(RSPInfo.DMEM + ((Address ^ 2) & 0xFFF));
    }
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rt].HW[0];
}

void RSP_Opcode_LW(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x3) != 0)
    {
        RSP_GPR[RSPOpC.rt].UW = *(uint8_t *)(RSPInfo.DMEM + (((Address + 0) & 0xFFF) ^ 3)) << 24;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) & 0xFFF) ^ 3)) << 16;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 2) & 0xFFF) ^ 3)) << 8;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 3) & 0xFFF) ^ 3)) << 0;
    }
    else
    {
        RSP_GPR[RSPOpC.rt].UW = *(uint32_t *)(RSPInfo.DMEM + (Address & 0xFFF));
    }
}

void RSP_Opcode_LBU(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    RSP_GPR[RSPOpC.rt].UW = *(uint8_t *)(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
}

void RSP_Opcode_LHU(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x1) != 0)
    {
        RSP_GPR[RSPOpC.rt].UHW[0] = *(uint8_t *)(RSPInfo.DMEM + (((Address + 0) & 0xFFF) ^ 3)) << 8;
        RSP_GPR[RSPOpC.rt].UHW[0] += *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) & 0xFFF) ^ 3)) << 0;
    }
    else
    {
        RSP_GPR[RSPOpC.rt].UHW[0] = *(uint16_t *)(RSPInfo.DMEM + ((Address ^ 2) & 0xFFF));
    }
    RSP_GPR[RSPOpC.rt].UW = RSP_GPR[RSPOpC.rt].UHW[0];
}

void RSP_Opcode_LWU(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x3) != 0)
    {
        RSP_GPR[RSPOpC.rt].UW = *(uint8_t *)(RSPInfo.DMEM + (((Address + 0) & 0xFFF) ^ 3)) << 24;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) & 0xFFF) ^ 3)) << 16;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 2) & 0xFFF) ^ 3)) << 8;
        RSP_GPR[RSPOpC.rt].UW += *(uint8_t *)(RSPInfo.DMEM + (((Address + 3) & 0xFFF) ^ 3)) << 0;
    }
    else
    {
        RSP_GPR[RSPOpC.rt].UW = *(uint32_t *)(RSPInfo.DMEM + (Address & 0xFFF));
    }
}

void RSP_Opcode_SB(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    *(uint8_t *)(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_GPR[RSPOpC.rt].UB[0];
}

void RSP_Opcode_SH(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x1) != 0)
    {
        *(uint8_t *)(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UHW[0] >> 8);
        *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UHW[0] & 0xFF);
    }
    else
    {
        *(uint16_t *)(RSPInfo.DMEM + ((Address ^ 2) & 0xFFF)) = RSP_GPR[RSPOpC.rt].UHW[0];
    }
}

void RSP_Opcode_SW(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (short)RSPOpC.offset) & 0xFFF;
    if ((Address & 0x3) != 0)
    {
        *(uint8_t *)(RSPInfo.DMEM + (((Address + 0) ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UW >> 24) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Address + 1) ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UW >> 16) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Address + 2) ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UW >> 8) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Address + 3) ^ 3) & 0xFFF)) = (RSP_GPR[RSPOpC.rt].UW >> 0) & 0xFF;
    }
    else
    {
        *(uint32_t *)(RSPInfo.DMEM + (Address & 0xFFF)) = RSP_GPR[RSPOpC.rt].UW;
    }
}

void RSP_Opcode_LC2(void)
{
    RSP_Lc2[RSPOpC.rd]();
}

void RSP_Opcode_SC2(void)
{
    RSP_Sc2[RSPOpC.rd]();
}

// R4300i Opcodes: Special

void RSP_Special_SLL(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W << RSPOpC.sa;
}

void RSP_Special_SRL(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rt].UW >> RSPOpC.sa;
}

void RSP_Special_SRA(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W >> RSPOpC.sa;
}

void RSP_Special_SLLV(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W << (RSP_GPR[RSPOpC.rs].W & 0x1F);
}

void RSP_Special_SRLV(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rt].UW >> (RSP_GPR[RSPOpC.rs].W & 0x1F);
}

void RSP_Special_SRAV(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W >> (RSP_GPR[RSPOpC.rs].W & 0x1F);
}

void RSP_Special_JR(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = (RSP_GPR[RSPOpC.rs].W & 0xFFC);
}

void RSP_Special_JALR(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = (RSP_GPR[RSPOpC.rs].W & 0xFFC);
    RSP_GPR[RSPOpC.rd].W = (*PrgCount + 8) & 0xFFC;
}

void RSP_Special_BREAK(void)
{
    RSP_Running = false;
    *RSPInfo.SP_STATUS_REG |= (SP_STATUS_HALT | SP_STATUS_BROKE);
    if ((*RSPInfo.SP_STATUS_REG & SP_STATUS_INTR_BREAK) != 0)
    {
        *RSPInfo.MI_INTR_REG |= MI_INTR_SP;
        RSPInfo.CheckInterrupts();
    }
}

void RSP_Special_ADD(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rs].W + RSP_GPR[RSPOpC.rt].W;
}

void RSP_Special_ADDU(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW + RSP_GPR[RSPOpC.rt].UW;
}

void RSP_Special_SUB(void)
{
    RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rs].W - RSP_GPR[RSPOpC.rt].W;
}

void RSP_Special_SUBU(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW - RSP_GPR[RSPOpC.rt].UW;
}

void RSP_Special_AND(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW & RSP_GPR[RSPOpC.rt].UW;
}

void RSP_Special_OR(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW | RSP_GPR[RSPOpC.rt].UW;
}

void RSP_Special_XOR(void)
{
    RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW ^ RSP_GPR[RSPOpC.rt].UW;
}

void RSP_Special_NOR(void)
{
    RSP_GPR[RSPOpC.rd].UW = ~(RSP_GPR[RSPOpC.rs].UW | RSP_GPR[RSPOpC.rt].UW);
}

void RSP_Special_SLT(void)
{
    RSP_GPR[RSPOpC.rd].UW = (RSP_GPR[RSPOpC.rs].W < RSP_GPR[RSPOpC.rt].W) ? 1 : 0;
}

void RSP_Special_SLTU(void)
{
    RSP_GPR[RSPOpC.rd].UW = (RSP_GPR[RSPOpC.rs].UW < RSP_GPR[RSPOpC.rt].UW) ? 1 : 0;
}

// R4300i Opcodes: RegImm

void RSP_Opcode_BLTZ(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W < 0);
}

void RSP_Opcode_BGEZ(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W >= 0);
}

void RSP_Opcode_BLTZAL(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W < 0);
    RSP_GPR[31].UW = (*PrgCount + 8) & 0xFFC;
}

void RSP_Opcode_BGEZAL(void)
{
    RSP_NextInstruction = RSPPIPELINE_DELAY_SLOT;
    RSP_JumpTo = RSP_branch_if(RSP_GPR[RSPOpC.rs].W >= 0);
    RSP_GPR[31].UW = (*PrgCount + 8) & 0xFFC;
}

// COP0 functions

void RSP_Cop0_MF(void)
{
    if (g_RSPDebugger != nullptr)
    {
        g_RSPDebugger->RDP_LogMF0(*PrgCount, RSPOpC.rd);
    }
    switch (RSPOpC.rd)
    {
    case 0: RSP_GPR[RSPOpC.rt].UW = g_RSPRegisterHandler->ReadReg(RSPRegister_MEM_ADDR); break;
    case 1: RSP_GPR[RSPOpC.rt].UW = g_RSPRegisterHandler->ReadReg(RSPRegister_DRAM_ADDR); break;
    case 2: RSP_GPR[RSPOpC.rt].UW = g_RSPRegisterHandler->ReadReg(RSPRegister_RD_LEN); break;
    case 3: RSP_GPR[RSPOpC.rt].UW = g_RSPRegisterHandler->ReadReg(RSPRegister_WR_LEN); break;
    case 4: RSP_GPR[RSPOpC.rt].UW = g_RSPRegisterHandler->ReadReg(RSPRegister_STATUS); break;
    case 5: RSP_GPR[RSPOpC.rt].UW = *RSPInfo.SP_DMA_FULL_REG; break;
    case 6: RSP_GPR[RSPOpC.rt].UW = *RSPInfo.SP_DMA_BUSY_REG; break;
    case 7:
        if (RspMultiThreaded)
        {
            RSP_GPR[RSPOpC.rt].W = *RSPInfo.SP_SEMAPHORE_REG;
            *RSPInfo.SP_SEMAPHORE_REG = 1;
        }
        else
        {
            RSP_GPR[RSPOpC.rt].W = 0;
        }
        break;
    case 8: RSP_GPR[RSPOpC.rt].UW = *RSPInfo.DPC_START_REG; break;
    case 9: RSP_GPR[RSPOpC.rt].UW = *RSPInfo.DPC_END_REG; break;
    case 10: RSP_GPR[RSPOpC.rt].UW = *RSPInfo.DPC_CURRENT_REG; break;
    case 11: RSP_GPR[RSPOpC.rt].W = *RSPInfo.DPC_STATUS_REG; break;
    case 12: RSP_GPR[RSPOpC.rt].W = *RSPInfo.DPC_CLOCK_REG; break;
    default:
        g_Notify->DisplayError(stdstr_f("We have not implemented RSP MF CP0 reg %s (%d)", COP0_Name(RSPOpC.rd), RSPOpC.rd).c_str());
    }
}

void RSP_Cop0_MT(void)
{
    if (LogRDP && g_CPUCore == InterpreterCPU)
    {
        RDP_LogMT0(*PrgCount, RSPOpC.rd, RSP_GPR[RSPOpC.rt].UW);
    }
    switch (RSPOpC.rd)
    {
    case 0: g_RSPRegisterHandler->WriteReg(RSPRegister_MEM_ADDR, RSP_GPR[RSPOpC.rt].UW); break;
    case 1: g_RSPRegisterHandler->WriteReg(RSPRegister_DRAM_ADDR, RSP_GPR[RSPOpC.rt].UW); break;
    case 2: g_RSPRegisterHandler->WriteReg(RSPRegister_RD_LEN, RSP_GPR[RSPOpC.rt].UW); break;
    case 3: g_RSPRegisterHandler->WriteReg(RSPRegister_WR_LEN, RSP_GPR[RSPOpC.rt].UW); break;
    case 4: g_RSPRegisterHandler->WriteReg(RSPRegister_STATUS, RSP_GPR[RSPOpC.rt].UW); break;
    case 7: *RSPInfo.SP_SEMAPHORE_REG = 0; break;
    case 8:
        *RSPInfo.DPC_START_REG = RSP_GPR[RSPOpC.rt].UW;
        *RSPInfo.DPC_CURRENT_REG = RSP_GPR[RSPOpC.rt].UW;
        break;
    case 9:
        *RSPInfo.DPC_END_REG = RSP_GPR[RSPOpC.rt].UW;
        RDP_LogDlist();
        if (RSPInfo.ProcessRdpList != NULL)
        {
            RSPInfo.ProcessRdpList();
        }
        break;
    case 10: *RSPInfo.DPC_CURRENT_REG = RSP_GPR[RSPOpC.rt].UW; break;
    case 11:
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_XBUS_DMEM_DMA) != 0)
        {
            *RSPInfo.DPC_STATUS_REG &= ~DPC_STATUS_XBUS_DMEM_DMA;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_SET_XBUS_DMEM_DMA) != 0)
        {
            *RSPInfo.DPC_STATUS_REG |= DPC_STATUS_XBUS_DMEM_DMA;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_FREEZE) != 0)
        {
            *RSPInfo.DPC_STATUS_REG &= ~DPC_STATUS_FREEZE;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_SET_FREEZE) != 0)
        {
            *RSPInfo.DPC_STATUS_REG |= DPC_STATUS_FREEZE;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_FLUSH) != 0)
        {
            *RSPInfo.DPC_STATUS_REG &= ~DPC_STATUS_FLUSH;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_SET_FLUSH) != 0)
        {
            *RSPInfo.DPC_STATUS_REG |= DPC_STATUS_FLUSH;
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_TMEM_CTR) != 0)
        { /* DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_TMEM_CTR"); */
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_PIPE_CTR) != 0)
        {
            g_Notify->DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_PIPE_CTR");
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_CMD_CTR) != 0)
        {
            g_Notify->DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_CMD_CTR");
        }
        if ((RSP_GPR[RSPOpC.rt].W & DPC_CLR_CLOCK_CTR) != 0)
        { /* DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_CLOCK_CTR"); */
        }
        break;
    default:
        g_Notify->DisplayError(stdstr_f("We have not implemented RSP MT CP0 reg %s (%d)", COP0_Name(RSPOpC.rd), RSPOpC.rd).c_str());
    }
}

// COP2 functions

void RSP_Cop2_MF(void)
{
    uint8_t element = (uint8_t)(RSPOpC.sa >> 1);
    RSP_GPR[RSPOpC.rt].B[1] = RSP_Vect[RSPOpC.vs].s8(15 - element);
    RSP_GPR[RSPOpC.rt].B[0] = RSP_Vect[RSPOpC.vs].s8(15 - ((element + 1) % 16));
    RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rt].HW[0];
}

void RSP_Cop2_CF(void)
{
    switch ((RSPOpC.rd & 0x03))
    {
    case 0: RSP_GPR[RSPOpC.rt].W = RSP_Flags[0].HW[0]; break;
    case 1: RSP_GPR[RSPOpC.rt].W = RSP_Flags[1].HW[0]; break;
    case 2: RSP_GPR[RSPOpC.rt].W = RSP_Flags[2].HW[0]; break;
    case 3: RSP_GPR[RSPOpC.rt].W = RSP_Flags[2].HW[0]; break;
    }
}

void RSP_Cop2_MT(void)
{
    uint8_t element = (uint8_t)(15 - (RSPOpC.sa >> 1));
    RSP_Vect[RSPOpC.vs].s8(element) = RSP_GPR[RSPOpC.rt].B[1];
    if (element != 0)
    {
        RSP_Vect[RSPOpC.vs].s8(element - 1) = RSP_GPR[RSPOpC.rt].B[0];
    }
}

void RSP_Cop2_CT(void)
{
    switch ((RSPOpC.rd & 0x03))
    {
    case 0: RSP_Flags[0].HW[0] = RSP_GPR[RSPOpC.rt].HW[0]; break;
    case 1: RSP_Flags[1].HW[0] = RSP_GPR[RSPOpC.rt].HW[0]; break;
    case 2: RSP_Flags[2].B[0] = RSP_GPR[RSPOpC.rt].B[0]; break;
    case 3: RSP_Flags[2].B[0] = RSP_GPR[RSPOpC.rt].B[0]; break;
    }
}

void RSP_COP2_VECTOR(void)
{
    RSP_Vector[RSPOpC.funct]();
}

// Vector functions

void RSP_Vector_VMULF(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, ((int64_t)RSP_Vect[RSPOpC.vs].s16(el) * (int64_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) * 2) + 0x8000);
        Result.s16(el) = AccumulatorSaturate(el, true);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMULU(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, ((int64_t)RSP_Vect[RSPOpC.vs].s16(el) * (int64_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) * 2) + 0x8000);
        if (RSP_ACCUM[el].HW[3] < 0)
        {
            Result.s16(el) = 0;
        }
        else if ((RSP_ACCUM[el].HW[3] ^ RSP_ACCUM[el].HW[2]) < 0)
        {
            Result.s16(el) = -1;
        }
        else
        {
            Result.s16(el) = RSP_ACCUM[el].HW[2];
        }
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VRNDP(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Value = RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        if (RSPOpC.vs & 1)
        {
            Value <<= 16;
        }
        int64_t Accum = AccumulatorGet(el);
        if (Accum >= 0)
        {
            Accum = clip48(Accum + Value);
        }
        AccumulatorSet(el, Accum);
        Result.s16(el) = clamp16((int32_t)(Accum >> 16));
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMUDL(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, (uint16_t)((uint32_t)RSP_Vect[RSPOpC.vs].u16(el) * (uint32_t)RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) >> 16));
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMUDM(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, (int32_t)((int32_t)RSP_Vect[RSPOpC.vs].s16(el) * (uint32_t)RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e)));
        Result.s16(el) = RSP_ACCUM[el].HW[2];
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMULQ(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Temp = RSP_Vect[RSPOpC.vs].s16(el) * RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        if (Temp < 0)
        {
            Temp += 31;
        }
        RSP_ACCUM[el].HW[3] = (int16_t)(Temp >> 16);
        RSP_ACCUM[el].HW[2] = (int16_t)Temp;
        RSP_ACCUM[el].HW[1] = 0;

        Result.s16(el) = clamp16(Temp >> 1) & ~15;
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMUDN(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, (int32_t)((uint32_t)RSP_Vect[RSPOpC.vs].u16(el) * (uint32_t)((int32_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e))));
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMUDH(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        RSP_ACCUM[el].W[1] = (int32_t)RSP_Vect[RSPOpC.vs].s16(el) * (int32_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = 0;
        Result.u16(el) = AccumulatorSaturate(el, true);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMACF(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, AccumulatorGet(el) + (((int64_t)RSP_Vect[RSPOpC.vs].s16(el) * (int64_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e)) << 1));
        Result.u16(el) = AccumulatorSaturate(el, true);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMACU(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, AccumulatorGet(el) + (((int64_t)RSP_Vect[RSPOpC.vs].s16(el) * (int64_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e)) << 1));
        if (RSP_ACCUM[el].HW[3] < 0)
        {
            Result.s16(el) = 0;
        }
        else if (RSP_ACCUM[el].UHW[3] != 0 || RSP_ACCUM[el].HW[2] < 0)
        {
            Result.u16(el) = 0xFFFF;
        }
        else
        {
            Result.s16(el) = RSP_ACCUM[el].HW[2];
        }
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMACQ(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Accum = (RSP_ACCUM[el].UHW[3] << 16) | RSP_ACCUM[el].UHW[2];
        if (Accum < -0x20 && ((Accum & 0x20) == 0))
        {
            Accum += 0x20;
        }
        else if (Accum > 0x20 && (Accum & 0x20) == 0)
        {
            Accum -= 0x20;
        }
        Result.u16(el) = clamp16(Accum >> 1) & 0xFFF0;
        RSP_ACCUM[el].UHW[3] = (uint16_t)(Accum >> 16);
        RSP_ACCUM[el].UHW[2] = (uint16_t)Accum;
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VRNDN(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Value = RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        if (RSPOpC.vs & 1)
        {
            Value <<= 16;
        }
        int64_t Accum = AccumulatorGet(el);
        if (Accum < 0)
        {
            Accum = clip48(Accum + Value);
        }
        AccumulatorSet(el, Accum);
        Result.s16(el) = clamp16((int32_t)(Accum >> 16));
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMADL(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, AccumulatorGet(el) + (((uint32_t)(RSP_Vect[RSPOpC.vs].u16(el)) * (uint32_t)RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e)) >> 16));
        Result.u16(el) = AccumulatorSaturate(el, false);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMADM(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, AccumulatorGet(el) + (RSP_Vect[RSPOpC.vs].s16(el) * RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e)));
        Result.u16(el) = AccumulatorSaturate(el, true);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMADN(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        AccumulatorSet(el, AccumulatorGet(el) + (int64_t)(RSP_Vect[RSPOpC.vs].u16(el) * RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e)));
        Result.u16(el) = AccumulatorSaturate(el, false);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VMADH(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Value = (int32_t)((AccumulatorGet(el) >> 16) + (int32_t)RSP_Vect[RSPOpC.vs].s16(el) * (int32_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
        RSP_ACCUM[el].HW[3] = (int16_t)(Value >> 16);
        RSP_ACCUM[el].HW[2] = (int16_t)(Value >> 0);
        Result.u16(el) = AccumulatorSaturate(el, true);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VADD(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Value = (int32_t)RSP_Vect[RSPOpC.vs].s16(el) + (int32_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) + VCOL.Get(el);
        RSP_ACCUM[el].HW[1] = (int16_t)Value;
        Result.u16(el) = clamp16(Value);
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VSUB(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Value = (int32_t)RSP_Vect[RSPOpC.vs].s16(el) - (int32_t)RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) - VCOL.Get(el);
        RSP_ACCUM[el].HW[1] = (int16_t)Value;
        Result.u16(el) = clamp16(Value);
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VABS(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if (RSP_Vect[RSPOpC.vs].s16(el) > 0)
        {
            Result.s16(el) = RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
            RSP_ACCUM[el].UHW[1] = Result.u16(el);
        }
        else if (RSP_Vect[RSPOpC.vs].s16(el) < 0)
        {
            if (RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) == 0x8000)
            {
                Result.u16(el) = 0x7FFF;
                RSP_ACCUM[el].UHW[1] = 0x8000;
            }
            else
            {
                Result.u16(el) = RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) * -1;
                RSP_ACCUM[el].UHW[1] = Result.u16(el);
            }
        }
        else
        {
            Result.u16(el) = 0;
            RSP_ACCUM[el].UHW[1] = 0;
        }
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VADDC(void)
{
    RSPVector Result;
    VCOH.Clear();
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Temp = (int32_t)RSP_Vect[RSPOpC.vs].u16(el) + (int32_t)RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = (int16_t)Temp;
        Result.u16(el) = RSP_ACCUM[el].HW[1];
        VCOL.Set(el, (Temp >> 16) != 0);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VSUBC(void)
{
    RSPVector Result;
    VCOH.Clear();
    for (uint8_t el = 0; el < 8; el++)
    {
        int32_t Temp = (int32_t)RSP_Vect[RSPOpC.vs].u16(el) - (int32_t)RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = (int16_t)Temp;
        Result.u16(el) = RSP_ACCUM[el].HW[1];
        VCOL.Set(el, (Temp >> 16) != 0);
        VCOH.Set(el, Temp != 0);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_Reserved(void)
{
    for (uint8_t el = 0; el < 8; el++)
    {
        RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.vs].s16(el) + RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
    }
    RSP_Vect[RSPOpC.vd] = RSPVector();
}

void RSP_Vector_VSAW(void)
{
    RSPVector Result;

    switch ((RSPOpC.rs & 0xF))
    {
    case 8:
        Result.s16(0) = RSP_ACCUM[0].HW[3];
        Result.s16(1) = RSP_ACCUM[1].HW[3];
        Result.s16(2) = RSP_ACCUM[2].HW[3];
        Result.s16(3) = RSP_ACCUM[3].HW[3];
        Result.s16(4) = RSP_ACCUM[4].HW[3];
        Result.s16(5) = RSP_ACCUM[5].HW[3];
        Result.s16(6) = RSP_ACCUM[6].HW[3];
        Result.s16(7) = RSP_ACCUM[7].HW[3];
        break;
    case 9:
        Result.s16(0) = RSP_ACCUM[0].HW[2];
        Result.s16(1) = RSP_ACCUM[1].HW[2];
        Result.s16(2) = RSP_ACCUM[2].HW[2];
        Result.s16(3) = RSP_ACCUM[3].HW[2];
        Result.s16(4) = RSP_ACCUM[4].HW[2];
        Result.s16(5) = RSP_ACCUM[5].HW[2];
        Result.s16(6) = RSP_ACCUM[6].HW[2];
        Result.s16(7) = RSP_ACCUM[7].HW[2];
        break;
    case 10:
        Result.s16(0) = RSP_ACCUM[0].HW[1];
        Result.s16(1) = RSP_ACCUM[1].HW[1];
        Result.s16(2) = RSP_ACCUM[2].HW[1];
        Result.s16(3) = RSP_ACCUM[3].HW[1];
        Result.s16(4) = RSP_ACCUM[4].HW[1];
        Result.s16(5) = RSP_ACCUM[5].HW[1];
        Result.s16(6) = RSP_ACCUM[6].HW[1];
        Result.s16(7) = RSP_ACCUM[7].HW[1];
        break;
    default:
        Result.u64(1) = 0;
        Result.u64(0) = 0;
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VLT(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if (RSP_Vect[RSPOpC.vs].s16(el) < RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) || (RSP_Vect[RSPOpC.vs].s16(el) == RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) && VCOL.Get(el) && VCOH.Get(el)))
        {
            Result.u16(el) = RSP_Vect[RSPOpC.vs].u16(el);
            VCCL.Set(el, true);
        }
        else
        {
            Result.u16(el) = RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
            VCCL.Set(el, false);
        }
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCCH.Clear();
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VEQ(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        RSP_ACCUM[el].HW[1] = VCCL.Set(el, RSP_Vect[RSPOpC.vs].u16(el) == RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) && !VCOH.Get(el)) ? RSP_Vect[RSPOpC.vs].u16(el) : RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
        Result.u16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCOL.Clear();
    VCOH.Clear();
    VCCH.Clear();
}

void RSP_Vector_VNE(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        RSP_ACCUM[el].HW[1] = VCCL.Set(el, RSP_Vect[RSPOpC.vs].u16(el) != RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) || VCOH.Get(el)) ? RSP_Vect[RSPOpC.vs].u16(el) : RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
        Result.u16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCCH.Clear();
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VGE(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if (RSP_Vect[RSPOpC.vs].s16(el) > RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) || (RSP_Vect[RSPOpC.vs].s16(el) == RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) && (!VCOL.Get(el) || !VCOH.Get(el))))
        {
            RSP_ACCUM[el].UHW[1] = RSP_Vect[RSPOpC.vs].u16(el);
            VCCL.Set(el, true);
        }
        else
        {
            RSP_ACCUM[el].UHW[1] = RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e);
            VCCL.Set(el, false);
        }
        Result.u16(el) = RSP_ACCUM[el].UHW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCCH.Clear();
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VCL(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if (VCOL.Get(el))
        {
            if (VCOH.Get(el))
            {
                RSP_ACCUM[el].HW[1] = VCCL.Get(el) ? -RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
            }
            else
            {
                bool Set = VCE.Get(el) ? (RSP_Vect[RSPOpC.vs].u16(el) + RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) <= 0x10000) : (RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) + RSP_Vect[RSPOpC.vs].u16(el) == 0);
                RSP_ACCUM[el].HW[1] = Set ? -RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
                VCCL.Set(el, Set);
            }
        }
        else
        {
            if (VCOH.Get(el))
            {
                RSP_ACCUM[el].UHW[1] = VCCH.Get(el) ? RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
            }
            else
            {
                RSP_ACCUM[el].HW[1] = VCCH.Set(el, RSP_Vect[RSPOpC.vs].u16(el) - RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) >= 0) ? RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
            }
        }
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    VCOL.Clear();
    VCOH.Clear();
    VCE.Clear();
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VCH(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if (VCOL.Set(el, (RSP_Vect[RSPOpC.vs].s16(el) ^ RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e)) < 0))
        {
            int16_t Value = RSP_Vect[RSPOpC.vs].s16(el) + RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
            RSP_ACCUM[el].HW[1] = Value <= 0 ? -RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
            VCOH.Set(el, Value != 0 && RSP_Vect[RSPOpC.vs].s16(el) != ~RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
            VCCL.Set(el, Value <= 0);
            VCCH.Set(el, RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) < 0);
            VCE.Set(el, Value == -1);
        }
        else
        {
            int16_t Value = RSP_Vect[RSPOpC.vs].s16(el) - RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
            RSP_ACCUM[el].HW[1] = Value >= 0 ? RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].s16(el);
            VCOH.Set(el, Value != 0 && RSP_Vect[RSPOpC.vs].s16(el) != ~RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
            VCCL.Set(el, RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) < 0);
            VCCH.Set(el, Value >= 0);
            VCE.Set(el, false);
        }
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VCR(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        if ((RSP_Vect[RSPOpC.vs].s16(el) ^ RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e)) < 0)
        {
            VCCH.Set(el, RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) < 0);
            RSP_ACCUM[el].HW[1] = VCCL.Set(el, RSP_Vect[RSPOpC.vs].s16(el) + RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) + 1 <= 0) ? ~RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].u16(el);
        }
        else
        {
            VCCL.Set(el, RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) < 0);
            RSP_ACCUM[el].HW[1] = VCCH.Set(el, RSP_Vect[RSPOpC.vs].s16(el) - RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e) >= 0) ? RSP_Vect[RSPOpC.vt].ue(el, RSPOpC.e) : RSP_Vect[RSPOpC.vs].u16(el);
        }
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCOL.Clear();
    VCOH.Clear();
    VCE.Clear();
}

void RSP_Vector_VMRG(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        RSP_ACCUM[el].HW[1] = VCCL.Get(el) ? RSP_Vect[RSPOpC.vs].s16(el) : RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        Result.s16(el) = RSP_ACCUM[el].HW[1];
    }
    RSP_Vect[RSPOpC.vd] = Result;
    VCOL.Clear();
    VCOH.Clear();
}

void RSP_Vector_VAND(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = RSP_Vect[RSPOpC.vs].s16(el) & RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VNAND(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = ~(RSP_Vect[RSPOpC.vs].s16(el) & RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VOR(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = RSP_Vect[RSPOpC.vs].s16(el) | RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VNOR(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = ~(RSP_Vect[RSPOpC.vs].s16(el) | RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VXOR(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = RSP_Vect[RSPOpC.vs].s16(el) ^ RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e);
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VNXOR(void)
{
    RSPVector Result;
    for (uint8_t el = 0; el < 8; el++)
    {
        Result.s16(el) = ~(RSP_Vect[RSPOpC.vs].s16(el) ^ RSP_Vect[RSPOpC.vt].se(el, RSPOpC.e));
        RSP_ACCUM[el].HW[1] = Result.s16(el);
    }
    RSP_Vect[RSPOpC.vd] = Result;
}

void RSP_Vector_VRCP(void)
{
    int32_t Input = RSP_Vect[RSPOpC.vt].s16(7 - (RSPOpC.e & 0x7));
    int32_t Mask = Input >> 31;
    int32_t Data = Input ^ Mask;
    if (Input > -32768)
    {
        Data -= Mask;
    }
    int32_t Result = 0;
    if (Data == 0)
    {
        Result = 0x7fffffff;
    }
    else if (Input == 0xFFFF8000)
    {
        Result = 0xffff0000;
    }
    else
    {
        uint32_t Shift = clz32(Data);
        uint32_t Index = (uint64_t(Data) << Shift & 0x7fc00000) >> 22;
        Result = (((0x10000 | Reciprocals[Index]) << 14) >> (31 - Shift)) ^ Mask;
    }
    RcpHigh = false;
    RcpResult = Result >> 16;
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[i]);
    }
    RSP_Vect[RSPOpC.vd].s16(7 - (RSPOpC.rd & 0x7)) = (int16_t)Result;
}

void RSP_Vector_VRCPL(void)
{
    int32_t Result = 0;
    int32_t Input = RcpHigh ? (RcpIn << 16 | RSP_Vect[RSPOpC.vt].u16(7 - (RSPOpC.e & 0x7))) : RSP_Vect[RSPOpC.vt].s16(7 - (RSPOpC.e & 0x7));
    int32_t Mask = Input >> 31;
    int32_t Data = Input ^ Mask;
    if (Input > -32768)
    {
        Data -= Mask;
    }
    if (Data == 0)
    {
        Result = 0x7fffffff;
    }
    else if (Input == 0xFFFF8000)
    {
        Result = 0xffff0000;
    }
    else
    {
        uint32_t Shift = clz32(Data);
        uint32_t Index = (uint64_t(Data) << Shift & 0x7fc00000) >> 22;
        Result = (((0x10000 | Reciprocals[Index]) << 14) >> (31 - Shift)) ^ Mask;
    }
    RcpHigh = false;
    RcpResult = Result >> 16;
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[i]);
    }
    RSP_Vect[RSPOpC.vd].s16(7 - (RSPOpC.rd & 0x7)) = (int16_t)Result;
}

void RSP_Vector_VRCPH(void)
{
    RcpHigh = true;
    RcpIn = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[(RSPOpC.de & 0x7)]);
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[i]);
    }
    RSP_Vect[RSPOpC.vd].u16(7 - (RSPOpC.de & 0x7)) = RcpResult;
}

void RSP_Vector_VMOV(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].ue(i, RSPOpC.e);
    }
    uint8_t Index = 7 - (RSPOpC.de & 0x7);
    RSP_Vect[RSPOpC.vd].u16(Index) = RSP_Vect[RSPOpC.vt].se(Index, RSPOpC.e);
}

void RSP_Vector_VRSQ(void)
{
    int64_t Result = 0;
    int32_t Input = RSP_Vect[RSPOpC.vt].s16(7 - (RSPOpC.e & 0x7));
    int32_t Mask = Input >> 31;
    int32_t Data = Input ^ Mask;
    if (Input > -32768)
    {
        Data -= Mask;
    }
    if (Data == 0)
    {
        Result = 0x7fffffff;
    }
    else if (Input == 0xFFFF8000)
    {
        Result = 0xffff0000;
    }
    else
    {
        uint32_t Shift = clz32(Data);
        uint32_t Index = (uint64_t(Data) << Shift & 0x7fc00000) >> 22;
        Result = (((0x10000 | InverseSquareRoots[(Index & 0x1fe) | (Shift & 1)]) << 14) >> ((31 - Shift) >> 1)) ^ Mask;
    }
    RcpHigh = false;
    RcpResult = (int16_t)(Result >> 16);
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].ue(i, RSPOpC.e);
    }
    RSP_Vect[RSPOpC.vd].s16(7 - (RSPOpC.rd & 0x7)) = (int16_t)Result;
}

void RSP_Vector_VRSQL(void)
{
    int32_t Result = 0;
    int32_t Input = RcpHigh ? RcpIn << 16 | RSP_Vect[RSPOpC.vt].u16(7 - (RSPOpC.e & 0x7)) : RSP_Vect[RSPOpC.vt].s16(7 - (RSPOpC.e & 0x7));
    int32_t Mask = Input >> 31;
    int32_t Data = Input ^ Mask;
    if (Input > -32768)
    {
        Data -= Mask;
    }
    if (Data == 0)
    {
        Result = 0x7fffffff;
    }
    else if (Input == 0xFFFF8000)
    {
        Result = 0xffff0000;
    }
    else
    {
        uint32_t Shift = clz32(Data);
        uint32_t Index = (uint64_t(Data) << Shift & 0x7fc00000) >> 22;
        Result = (((0x10000 | InverseSquareRoots[(Index & 0x1fe) | (Shift & 1)]) << 14) >> ((31 - Shift) >> 1)) ^ Mask;
    }
    RcpHigh = 0;
    RcpResult = Result >> 16;
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[i]);
    }
    RSP_Vect[RSPOpC.vd].s16(7 - (RSPOpC.rd & 0x7)) = (int16_t)Result;
}

void RSP_Vector_VRSQH(void)
{
    RcpHigh = 1;
    RcpIn = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[(RSPOpC.rd & 0x7)]);
    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_ACCUM[i].HW[1] = RSP_Vect[RSPOpC.vt].u16(EleSpec[RSPOpC.e].B[i]);
    }
    RSP_Vect[RSPOpC.vd].u16(7 - (RSPOpC.rd & 0x7)) = RcpResult;
}

void RSP_Vector_VNOOP(void)
{
}

// LC2 functions

void RSP_Opcode_LBV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 0)) & 0xFFF;
    RSP_Vect[RSPOpC.vt].u8((uint8_t)(15 - RSPOpC.del)) = *(RSPInfo.DMEM + (Address ^ 3));
}

void RSP_Opcode_LSV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 1)) & 0xFFF;
    uint8_t Length = std::min((uint8_t)2, (uint8_t)(16 - RSPOpC.del));
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(Length + RSPOpC.del); i < n; i++, Address++)
    {
        RSP_Vect[RSPOpC.vt].u8(15 - i) = *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
    }
}

void RSP_Opcode_LLV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 2)) & 0xFFF;
    uint8_t Length = std::min((uint8_t)4, (uint8_t)(16 - RSPOpC.del));
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(Length + RSPOpC.del); i < n; i++, Address++)
    {
        RSP_Vect[RSPOpC.vt].u8(15 - i) = *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
    }
}

void RSP_Opcode_LDV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3)) & 0xFFF;
    uint8_t Length = std::min((uint8_t)8, (uint8_t)(16 - RSPOpC.del));
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(Length + RSPOpC.del); i < n; i++, Address++)
    {
        RSP_Vect[RSPOpC.vt].u8(15 - i) = *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
    }
}

void RSP_Opcode_LQV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Length = std::min((uint8_t)(((Address + 0x10) & ~0xF) - Address), (uint8_t)(16 - RSPOpC.del));
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(Length + RSPOpC.del); i < n; i++, Address++)
    {
        RSP_Vect[RSPOpC.vt].u8(15 - i) = *(RSPInfo.DMEM + (Address ^ 3));
    }
}

void RSP_Opcode_LRV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Offset = (uint8_t)((0x10 - (Address & 0xF)) + RSPOpC.del);
    Address &= 0xFF0;
    for (uint8_t i = Offset; i < 16; i++, Address++)
    {
        RSP_Vect[RSPOpC.vt].u8(15 - i) = *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF));
    }
}

void RSP_Opcode_LPV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3));
    uint32_t Offset = ((Address & 7) - RSPOpC.del);
    Address &= ~7;

    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_Vect[RSPOpC.vt].u16(7 - i) = *(RSPInfo.DMEM + ((Address + ((Offset + i) & 0xF) ^ 3) & 0xFFF)) << 8;
    }
}

void RSP_Opcode_LUV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3));
    uint32_t Offset = ((Address & 7) - RSPOpC.del);
    Address &= ~7;

    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_Vect[RSPOpC.vt].s16(7 - i) = *(RSPInfo.DMEM + ((Address + ((Offset + i) & 0xF) ^ 3) & 0xFFF)) << 7;
    }
}

void RSP_Opcode_LHV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4));
    uint32_t Offset = ((Address & 7) - RSPOpC.del);
    Address &= ~7;

    for (uint8_t i = 0; i < 8; i++)
    {
        RSP_Vect[RSPOpC.vt].s16(7 - i) = *(RSPInfo.DMEM + ((Address + ((Offset + (i << 1)) & 0xF) ^ 3) & 0xFFF)) << 7;
    }
}

void RSP_Opcode_LFV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4));
    uint8_t Length = std::min((uint8_t)(8 + RSPOpC.del), (uint8_t)16);
    uint32_t Offset = ((Address & 7) - RSPOpC.del);
    Address &= ~7;

    RSPVector Temp;
    for (uint8_t i = 0; i < 4; i++)
    {
        Temp.s16(i) = *(RSPInfo.DMEM + ((Address + ((Offset + (i << 2)) & 0xF) ^ 3) & 0xFFF)) << 7;
        Temp.s16(i + 4) = *(RSPInfo.DMEM + ((Address + ((Offset + (i << 2) + 8) & 0xF) ^ 3) & 0xFFF)) << 7;
    }

    for (uint8_t i = RSPOpC.del; i < Length; i++)
    {
        RSP_Vect[RSPOpC.vt].s8(15 - i) = Temp.s8(i ^ 1);
    }
}

void RSP_Opcode_LWV(void)
{
}

void RSP_Opcode_LTV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4));
    uint32_t Start = Address & ~7;
    uint32_t End = Start + 0x10;
    Address = Start + ((RSPOpC.del + (Address & 8)) & 0xF);
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t del = (((RSPOpC.del >> 1) + i) & 7) + (RSPOpC.rt & ~7);
        RSP_Vect[del].s8(15 - (i * 2 + 0)) = *(RSPInfo.DMEM + ((Address++ ^ 3) & 0xFFF));
        if (Address == End)
        {
            Address = Start;
        }
        RSP_Vect[del].s8(15 - (i * 2 + 1)) = *(RSPInfo.DMEM + ((Address++ ^ 3) & 0xFFF));
        if (Address == End)
        {
            Address = Start;
        }
    }
}

// SC2 functions

void RSP_Opcode_SBV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 0)) & 0xFFF;
    *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8((uint8_t)(15 - RSPOpC.del));
}

void RSP_Opcode_SSV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 1)) & 0xFFF;
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(2 + RSPOpC.del); i < n; i++, Address++)
    {
        *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - (i & 0xF));
    }
}

void RSP_Opcode_SLV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 2)) & 0xFFF;
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(4 + RSPOpC.del); i < n; i++, Address++)
    {
        *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - (i & 0xF));
    }
}

void RSP_Opcode_SDV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3)) & 0xFFF;
    for (uint8_t i = RSPOpC.del; i < (8 + RSPOpC.del); i++, Address++)
    {
        *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - (i & 0xF));
    }
}

void RSP_Opcode_SQV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Length = (uint8_t)(((Address + 0x10) & ~0xF) - Address);
    for (uint8_t i = RSPOpC.del; i < (Length + RSPOpC.del); i++, Address++)
    {
        *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - (i & 0xF));
    }
}

void RSP_Opcode_SRV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Length = (Address & 0xF);
    uint8_t Offset = (0x10 - Length) & 0xF;
    Address &= 0xFF0;
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(Length + RSPOpC.del); i < n; i++, Address++)
    {
        *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - ((i + Offset) & 0xF));
    }
}

void RSP_Opcode_SPV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3)) & 0xFFF;
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(8 + RSPOpC.del); i < n; i++, Address++)
    {
        if (((i)&0xF) < 8)
        {
            *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - ((i & 0xF) << 1));
        }
        else
        {
            *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u8(15 - ((i & 0x7) << 1)) << 1) + (RSP_Vect[RSPOpC.vt].u8(14 - ((i & 0x7) << 1)) >> 7);
        }
    }
}

void RSP_Opcode_SUV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 3)) & 0xFFF;
    for (uint8_t Count = RSPOpC.del; Count < (8 + RSPOpC.del); Count++, Address++)
    {
        if (((Count)&0xF) < 8)
        {
            *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = ((RSP_Vect[RSPOpC.vt].u8(15 - ((Count & 0x7) << 1)) << 1) + (RSP_Vect[RSPOpC.vt].u8(14 - ((Count & 0x7) << 1)) >> 7)) & 0xFF;
        }
        else
        {
            *(RSPInfo.DMEM + ((Address ^ 3) & 0xFFF)) = RSP_Vect[RSPOpC.vt].u8(15 - ((Count & 0x7) << 1));
        }
    }
}

void RSP_Opcode_SHV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4));
    uint8_t Offset = Address & 7;
    Address &= ~7;
    for (uint32_t i = 0; i < 16; i += 2)
    {
        uint8_t Value = (RSP_Vect[RSPOpC.vt].u8(15 - ((RSPOpC.del + i) & 15)) << 1) | (RSP_Vect[RSPOpC.vt].u8(15 - ((RSPOpC.del + i + 1) & 15)) >> 7);
        *(RSPInfo.DMEM + ((Address + (Offset + i & 15) ^ 3) & 0xFFF)) = Value;
    }
}

void RSP_Opcode_SFV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Offset = Address & 7;
    Address &= 0xFF8;

    switch (RSPOpC.del)
    {
    case 0:
    case 15:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(7) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(5) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(4) >> 7) & 0xFF;
        break;
    case 1:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(2) >> 7) & 0xFF;
        break;
    case 4:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(5) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(4) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(7) >> 7) & 0xFF;
        break;
    case 5:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(1) >> 7) & 0xFF;
        break;
    case 8:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(0) >> 7) & 0xFF;
        break;
    case 11:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(4) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(7) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(5) >> 7) & 0xFF;
        break;
    case 12:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF)) ^ 3) & 0xFFF)) = (RSP_Vect[RSPOpC.vt].u16(3) >> 7) & 0xFF;
        break;
    default:
        *(RSPInfo.DMEM + (((Address + Offset) ^ 3) & 0xFFF)) = 0;
        *(RSPInfo.DMEM + (((Address + ((Offset + 4) & 0xF)) ^ 3) & 0xFFF)) = 0;
        *(RSPInfo.DMEM + (((Address + ((Offset + 8) & 0xF) & 0xFFF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + (((Address + ((Offset + 12) & 0xF) & 0xFFF)) ^ 3)) = 0;
        break;
    }
}

void RSP_Opcode_STV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4));
    uint8_t Element = 16 - (RSPOpC.del & ~1);
    uint8_t Offset = (Address & 7) - (RSPOpC.del & ~1);
    Address &= ~7;
    for (uint32_t i = 0; i < 16; i += 2)
    {
        uint8_t Del = (uint8_t)((RSPOpC.vt & ~7) + (i >> 1));
        *(RSPInfo.DMEM + (((Address + (Offset + i & 15) ^ 3)) & 0xFFF)) = RSP_Vect[Del].s8(15 - ((Element + i) & 15));
        *(RSPInfo.DMEM + (((Address + (Offset + i + 1 & 15) ^ 3)) & 0xFFF)) = RSP_Vect[Del].s8(15 - ((Element + i + 1) & 15));
    }
}

void RSP_Opcode_SWV(void)
{
    uint32_t Address = (uint32_t)(RSP_GPR[RSPOpC.base].W + (RSPOpC.voffset << 4)) & 0xFFF;
    uint8_t Offset = Address & 7;
    Address &= 0xFF8;
    for (uint8_t i = RSPOpC.del, n = (uint8_t)(16 + RSPOpC.del); i < n; i++, Offset++)
    {
        *(RSPInfo.DMEM + ((Address + (Offset & 0xF)) ^ 3)) = RSP_Vect[RSPOpC.vt].s8(15 - (i & 0xF));
    }
}

// Other functions

void rsp_UnknownOpcode(void)
{
    if (g_RSPDebugger != nullptr)
    {
        g_RSPDebugger->UnknownOpcode();
    }
}
