#include "RspRecompilerCPU.h"
#include <Common/StdString.h>
#include <Project64-rsp-core/RSPInfo.h>
#include <Project64-rsp-core/Settings/RspSettings.h>
#include <Project64-rsp-core/cpu/RSPCpu.h>
#include <Project64-rsp-core/cpu/RSPInstruction.h>
#include <Project64-rsp-core/cpu/RSPOpcode.h>
#include <Project64-rsp-core/cpu/RspMemory.h>
#include <Project64-rsp-core/cpu/RspTypes.h>

//#define COMPARE_INSTRUCTIONS_VERBOSE

/*
IsOpcodeNop
Output: bool whether opcode at PC is a NOP
Input: PC
*/

bool IsOpcodeNop(uint32_t PC)
{
    RSPOpcode RspOp;
    RspOp.Value = *(uint32_t *)(RSPInfo.IMEM + (PC & 0xFFC));
    if (RspOp.op == RSP_SPECIAL && RspOp.funct == RSP_SPECIAL_SLL)
    {
        return (RspOp.rd == 0) ? true : false;
    }

    return false;
}

/*
IsRegisterConstant
Output:
True: Register is constant throughout
False: Register is not constant at all
Input: PC, Pointer to constant to fill
*/

bool IsRegisterConstant(uint32_t Reg, uint32_t * Constant)
{
    uint32_t PC = 0;
    uint32_t References = 0;
    uint32_t Const = 0;
    RSPOpcode RspOp;

    if (Compiler.bGPRConstants == false)
        return false;

    while (PC < 0x1000)
    {
        RspOp.Value = *(uint32_t *)(RSPInfo.IMEM + (PC & 0xFFC));

        // Resample command in microcode likes S7
        /*	if (PC == 0xFBC) {
			PC += 4;
			continue;
		}*/

        PC += 4;

        switch (RspOp.op)
        {
        case RSP_REGIMM:
            break;

        case RSP_SPECIAL:
            switch (RspOp.funct)
            {
            case RSP_SPECIAL_SLL:
            case RSP_SPECIAL_SRL:
            case RSP_SPECIAL_SRA:
            case RSP_SPECIAL_SLLV:
            case RSP_SPECIAL_SRLV:
            case RSP_SPECIAL_SRAV:
            case RSP_SPECIAL_ADD:
            case RSP_SPECIAL_ADDU:
            case RSP_SPECIAL_SUB:
            case RSP_SPECIAL_SUBU:
            case RSP_SPECIAL_AND:
            case RSP_SPECIAL_OR:
            case RSP_SPECIAL_XOR:
            case RSP_SPECIAL_NOR:
            case RSP_SPECIAL_SLT:
            case RSP_SPECIAL_SLTU:
                if (RspOp.rd == Reg)
                {
                    return false;
                }
                break;

            case RSP_SPECIAL_BREAK:
            case RSP_SPECIAL_JR:
                break;

            default:
                //	CompilerWarning(stdstr_f("Unknown opcode in IsRegisterConstant\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
                //	return false;
                break;
            }
            break;

        case RSP_J:
        case RSP_JAL:
        case RSP_BEQ:
        case RSP_BNE:
        case RSP_BLEZ:
        case RSP_BGTZ:
            break;

        case RSP_ADDI:
        case RSP_ADDIU:
            if (RspOp.rt == Reg)
            {
                if (RspOp.rs == 0)
                {
                    if (References > 0)
                    {
                        return false;
                    }
                    Const = (short)RspOp.immediate;
                    References++;
                }
                else
                {
                    return false;
                }
            }
            break;
        case RSP_ORI:
            if (RspOp.rt == Reg)
            {
                if (!RspOp.rs)
                {
                    if (References > 0)
                    {
                        return false;
                    }
                    Const = (uint16_t)RspOp.immediate;
                    References++;
                }
                else
                    return false;
            }
            break;

        case RSP_LUI:
            if (RspOp.rt == Reg)
            {
                if (References > 0)
                {
                    return false;
                }
                Const = (short)RspOp.offset << 16;
                References++;
            }
            break;

        case RSP_ANDI:
        case RSP_XORI:
        case RSP_SLTI:
        case RSP_SLTIU:
            if (RspOp.rt == Reg)
            {
                return false;
            }
            break;

        case RSP_CP0:
            switch (RspOp.rs)
            {
            case RSP_COP0_MF:
                if (RspOp.rt == Reg)
                {
                    return false;
                }
            case RSP_COP0_MT:
                break;
            }
            break;

        case RSP_CP2:
            if ((RspOp.rs & 0x10) == 0)
            {
                switch (RspOp.rs)
                {
                case RSP_COP2_CT:
                case RSP_COP2_MT:
                    break;

                case RSP_COP2_CF:
                case RSP_COP2_MF:
                    if (RspOp.rt == Reg)
                    {
                        return false;
                    }
                    break;

                default:
                    //	CompilerWarning(stdstr_f("Unknown opcode in IsRegisterConstant\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
                    //	return false;
                    break;
                }
            }
            break;

        case RSP_LB:
        case RSP_LH:
        case RSP_LW:
        case RSP_LBU:
        case RSP_LHU:
        case RSP_LWU:
            if (RspOp.rt == Reg)
            {
                return false;
            }
            break;

        case RSP_SB:
        case RSP_SH:
        case RSP_SW:
            break;
        case RSP_LC2:
            break;
        case RSP_SC2:
            break;
        default:
            //	CompilerWarning(stdstr_f("Unknown opcode in IsRegisterConstant\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
            //	return false;
            break;
        }
    }

    if (References > 0)
    {
        *Constant = Const;
        return true;
    }
    else
    {
        *Constant = 0;
        return false;
    }
}

/*
IsOpcodeBranch
Output:
True: Opcode is a branch
False: Opcode is not a branch
Input: PC
*/

bool IsOpcodeBranch(uint32_t PC, RSPOpcode RspOp)
{
    PC = PC; // Unused

    switch (RspOp.op)
    {
    case RSP_REGIMM:
        switch (RspOp.rt)
        {
        case RSP_REGIMM_BLTZ:
        case RSP_REGIMM_BGEZ:
        case RSP_REGIMM_BLTZAL:
        case RSP_REGIMM_BGEZAL:
            return true;
        default:
            //CompilerWarning(stdstr_f(stdstr_f("Unknown opcode in IsOpcodeBranch\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
            break;
        }
        break;
    case RSP_SPECIAL:
        switch (RspOp.funct)
        {
        case RSP_SPECIAL_SLL:
        case RSP_SPECIAL_SRL:
        case RSP_SPECIAL_SRA:
        case RSP_SPECIAL_SLLV:
        case RSP_SPECIAL_SRLV:
        case RSP_SPECIAL_SRAV:
        case RSP_SPECIAL_ADD:
        case RSP_SPECIAL_ADDU:
        case RSP_SPECIAL_SUB:
        case RSP_SPECIAL_SUBU:
        case RSP_SPECIAL_AND:
        case RSP_SPECIAL_OR:
        case RSP_SPECIAL_XOR:
        case RSP_SPECIAL_NOR:
        case RSP_SPECIAL_SLT:
        case RSP_SPECIAL_SLTU:
        case RSP_SPECIAL_BREAK:
            break;

        case RSP_SPECIAL_JALR:
        case RSP_SPECIAL_JR:
            return true;

        default:
            //CompilerWarning(stdstr_f("Unknown opcode in IsOpcodeBranch\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
            break;
        }
        break;
    case RSP_J:
    case RSP_JAL:
    case RSP_BEQ:
    case RSP_BNE:
    case RSP_BLEZ:
    case RSP_BGTZ:
        return true;

    case RSP_ADDI:
    case RSP_ADDIU:
    case RSP_SLTI:
    case RSP_SLTIU:
    case RSP_ANDI:
    case RSP_ORI:
    case RSP_XORI:
    case RSP_LUI:

    case RSP_CP0:
    case RSP_CP2:
        break;

    case RSP_LB:
    case RSP_LH:
    case RSP_LW:
    case RSP_LBU:
    case RSP_LHU:
    case RSP_SB:
    case RSP_SH:
    case RSP_SW:
        break;

    case RSP_LC2:
    case RSP_SC2:
        break;

    default:
        //CompilerWarning(stdstr_f("Unknown opcode in IsOpcodeBranch\n%s",RSPOpcodeName(RspOp.Hex,PC)).c_str());
        break;
    }

    return false;
}

/*
GetInstructionInfo
Output: None in regard to return value
Input: Pointer to info structure, fills this
with valid opcode data
*/

// 3 possible values, GPR, VEC, VEC and GPR, NOOP is zero
#define GPR_Instruction 0x0001     /* GPR Instruction flag */
#define VEC_Instruction 0x0002     /* Vec Instruction flag */
#define COPO_MF_Instruction 0x0080 /* MF Cop 0 Instruction */
#define Flag_Instruction 0x0100    /* Access Flags */
#define Instruction_Mask (GPR_Instruction | VEC_Instruction)

// 3 possible values, one flag must be set only
#define Load_Operation 0x0004  /* Load Instruction flag */
#define Store_Operation 0x0008 /* Store Instruction flag */
#define Accum_Operation 0x0010 /* Vector op uses accum - loads & stores dont */
#define MemOperation_Mask (Load_Operation | Store_Operation)
#define Operation_Mask (MemOperation_Mask | Accum_Operation)

// Per situation basis flags
#define VEC_ResetAccum 0x0000 /* Vector op resets acc */
#define VEC_Accumulate 0x0020 /* Vector op accumulates */

// N/A in instruction assembler syntax, possibly an unused register specifier
#define UNUSED_OPERAND ~0u

#define InvalidOpcode 0x0040

#pragma warning(push)
#pragma warning(disable : 4201) // Non-standard extension used: nameless struct/union

typedef struct
{
    union
    {
        uint32_t DestReg;
        uint32_t StoredReg;
    };
    union
    {
        uint32_t SourceReg0;
        uint32_t IndexReg;
    };
    uint32_t SourceReg1;
    uint32_t flags;
} OPCODE_INFO;

#pragma warning(pop)

void GetInstructionInfo(uint32_t PC, RSPOpcode * RspOp, OPCODE_INFO * info)
{
    switch (RspOp->op)
    {
    case RSP_REGIMM:
        switch (RspOp->rt)
        {
        case RSP_REGIMM_BLTZ:
        case RSP_REGIMM_BLTZAL:
        case RSP_REGIMM_BGEZ:
        case RSP_REGIMM_BGEZAL:
            info->flags = InvalidOpcode;
            info->SourceReg0 = RspOp->rs;
            info->SourceReg1 = UNUSED_OPERAND;
            break;

        default:
            CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
            info->flags = InvalidOpcode;
            break;
        }
        break;
    case RSP_SPECIAL:
        switch (RspOp->funct)
        {
        case RSP_SPECIAL_BREAK:
            info->DestReg = UNUSED_OPERAND;
            info->SourceReg0 = UNUSED_OPERAND;
            info->SourceReg1 = UNUSED_OPERAND;
            info->flags = GPR_Instruction;
            break;

        case RSP_SPECIAL_SLL:
        case RSP_SPECIAL_SRL:
        case RSP_SPECIAL_SRA:
            info->DestReg = RspOp->rd;
            info->SourceReg0 = RspOp->rt;
            info->SourceReg1 = UNUSED_OPERAND;
            info->flags = GPR_Instruction;
            break;
        case RSP_SPECIAL_SLLV:
        case RSP_SPECIAL_SRLV:
        case RSP_SPECIAL_SRAV:
        case RSP_SPECIAL_ADD:
        case RSP_SPECIAL_ADDU:
        case RSP_SPECIAL_SUB:
        case RSP_SPECIAL_SUBU:
        case RSP_SPECIAL_AND:
        case RSP_SPECIAL_OR:
        case RSP_SPECIAL_XOR:
        case RSP_SPECIAL_NOR:
        case RSP_SPECIAL_SLT:
        case RSP_SPECIAL_SLTU:
            info->DestReg = RspOp->rd;
            info->SourceReg0 = RspOp->rs;
            info->SourceReg1 = RspOp->rt;
            info->flags = GPR_Instruction;
            break;

        case RSP_SPECIAL_JR:
            info->flags = InvalidOpcode;
            info->SourceReg0 = UNUSED_OPERAND;
            info->SourceReg1 = UNUSED_OPERAND;
            break;

        default:
            CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
            info->flags = InvalidOpcode;
            break;
        }
        break;
    case RSP_J:
    case RSP_JAL:
        info->flags = InvalidOpcode;
        info->SourceReg0 = UNUSED_OPERAND;
        info->SourceReg1 = UNUSED_OPERAND;
        break;
    case RSP_BEQ:
    case RSP_BNE:
        info->flags = InvalidOpcode;
        info->SourceReg0 = RspOp->rt;
        info->SourceReg1 = RspOp->rs;
        break;
    case RSP_BLEZ:
    case RSP_BGTZ:
        info->flags = InvalidOpcode;
        info->SourceReg0 = RspOp->rs;
        info->SourceReg1 = UNUSED_OPERAND;
        break;

    case RSP_ADDI:
    case RSP_ADDIU:
    case RSP_SLTI:
    case RSP_SLTIU:
    case RSP_ANDI:
    case RSP_ORI:
    case RSP_XORI:
        info->DestReg = RspOp->rt;
        info->SourceReg0 = RspOp->rs;
        info->SourceReg1 = UNUSED_OPERAND;
        info->flags = GPR_Instruction;
        break;

    case RSP_LUI:
        info->DestReg = RspOp->rt;
        info->SourceReg0 = UNUSED_OPERAND;
        info->SourceReg1 = UNUSED_OPERAND;
        info->flags = GPR_Instruction;
        break;

    case RSP_CP0:
        switch (RspOp->rs)
        {
        case RSP_COP0_MF:
            info->DestReg = RspOp->rt;
            info->SourceReg0 = UNUSED_OPERAND;
            info->SourceReg1 = UNUSED_OPERAND;
            if (RspOp->rd == 0x4 || RspOp->rd == 0x7)
            {
                info->flags = InvalidOpcode | COPO_MF_Instruction;
            }
            else
            {
                info->flags = COPO_MF_Instruction | GPR_Instruction | Load_Operation;
            }
            break;

        case RSP_COP0_MT:
            info->StoredReg = RspOp->rt;
            info->SourceReg0 = UNUSED_OPERAND;
            info->SourceReg1 = UNUSED_OPERAND;
            info->flags = GPR_Instruction | Store_Operation;
            break;
        }
        break;

    case RSP_CP2:
        if ((RspOp->rs & 0x10) != 0)
        {
            switch (RspOp->funct)
            {
            case RSP_VECTOR_VNOP:
                info->DestReg = UNUSED_OPERAND;
                info->SourceReg0 = UNUSED_OPERAND;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = VEC_Instruction;
                break;

            case RSP_VECTOR_VMULF:
            case RSP_VECTOR_VMULU:
            case RSP_VECTOR_VMUDL:
            case RSP_VECTOR_VMUDM:
            case RSP_VECTOR_VMUDN:
            case RSP_VECTOR_VMUDH:
            case RSP_VECTOR_VABS:
            case RSP_VECTOR_VAND:
            case RSP_VECTOR_VOR:
            case RSP_VECTOR_VXOR:
            case RSP_VECTOR_VNAND:
            case RSP_VECTOR_VNOR:
            case RSP_VECTOR_VNXOR:
                info->DestReg = RspOp->sa;
                info->SourceReg0 = RspOp->rd;
                info->SourceReg1 = RspOp->rt;
                info->flags = VEC_Instruction | VEC_ResetAccum | Accum_Operation;
                break;
            case RSP_VECTOR_VMACF:
            case RSP_VECTOR_VMACU:
            case RSP_VECTOR_VMADL:
            case RSP_VECTOR_VMADM:
            case RSP_VECTOR_VMADN:
            case RSP_VECTOR_VMADH:
                info->DestReg = RspOp->sa;
                info->SourceReg0 = RspOp->rd;
                info->SourceReg1 = RspOp->rt;
                info->flags = VEC_Instruction | VEC_Accumulate | Accum_Operation;
                break;
            case RSP_VECTOR_VADD:
            case RSP_VECTOR_VADDC:
            case RSP_VECTOR_VSUB:
            case RSP_VECTOR_VSUBC:
            case RSP_VECTOR_VCR:
            case RSP_VECTOR_VCH:
            case RSP_VECTOR_VCL:
            case RSP_VECTOR_VLT:
            case RSP_VECTOR_VEQ:
            case RSP_VECTOR_VGE:
            case RSP_VECTOR_VNE:
                info->DestReg = RspOp->sa;
                info->SourceReg0 = RspOp->rd;
                info->SourceReg1 = RspOp->rt;
                info->flags = VEC_Instruction | VEC_ResetAccum | Accum_Operation | Flag_Instruction;
                break;

            case RSP_VECTOR_VMOV:
            case RSP_VECTOR_VRCP:
            case RSP_VECTOR_VRCPL:
            case RSP_VECTOR_VRCPH:
            case RSP_VECTOR_VRSQL:
            case RSP_VECTOR_VRSQH:
                info->DestReg = RspOp->sa;
                info->SourceReg0 = RspOp->rt;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = VEC_Instruction | VEC_ResetAccum | Accum_Operation; // Assume reset?
                break;

            case RSP_VECTOR_VMRG:
                info->DestReg = RspOp->sa;
                info->SourceReg0 = RspOp->rt;
                info->SourceReg1 = RspOp->rd;
                info->flags = VEC_Instruction | VEC_ResetAccum | Accum_Operation | Flag_Instruction; // Assume reset?
                break;

            case RSP_VECTOR_VSAW:
                //	info->flags = InvalidOpcode;
                info->DestReg = RspOp->sa;
                info->SourceReg0 = UNUSED_OPERAND;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = VEC_Instruction | Accum_Operation | VEC_Accumulate;
                break;

            default:
                CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
                info->flags = InvalidOpcode;
                break;
            }
        }
        else
        {
            switch (RspOp->rs)
            {
            case RSP_COP2_CT:
                info->StoredReg = RspOp->rt;
                info->SourceReg0 = UNUSED_OPERAND;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = GPR_Instruction | Store_Operation | Flag_Instruction;
                break;
            case RSP_COP2_CF:
                info->DestReg = RspOp->rt;
                info->SourceReg0 = UNUSED_OPERAND;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = GPR_Instruction | Load_Operation | Flag_Instruction;
                break;

            // RD is always the vector register, RT is always GPR
            case RSP_COP2_MT:
                info->DestReg = RspOp->rd;
                info->SourceReg0 = RspOp->rt;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = VEC_Instruction | GPR_Instruction | Load_Operation;
                break;
            case RSP_COP2_MF:
                info->DestReg = RspOp->rt;
                info->SourceReg0 = RspOp->rd;
                info->SourceReg1 = UNUSED_OPERAND;
                info->flags = VEC_Instruction | GPR_Instruction | Store_Operation;
                break;
            default:
                CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
                info->flags = InvalidOpcode;
                break;
            }
        }
        break;
    case RSP_LB:
    case RSP_LH:
    case RSP_LW:
    case RSP_LBU:
    case RSP_LHU:
        info->DestReg = RspOp->rt;
        info->IndexReg = RspOp->base;
        info->SourceReg1 = UNUSED_OPERAND;
        info->flags = Load_Operation | GPR_Instruction;
        break;
    case RSP_SB:
    case RSP_SH:
    case RSP_SW:
        info->StoredReg = RspOp->rt;
        info->IndexReg = RspOp->base;
        info->SourceReg1 = UNUSED_OPERAND;
        info->flags = Store_Operation | GPR_Instruction;
        break;
    case RSP_LC2:
        switch (RspOp->rd)
        {
        case RSP_LSC2_BV:
        case RSP_LSC2_SV:
        case RSP_LSC2_DV:
        case RSP_LSC2_RV:
        case RSP_LSC2_QV:
        case RSP_LSC2_LV:
        case RSP_LSC2_UV:
        case RSP_LSC2_PV:
            info->DestReg = RspOp->rt;
            info->IndexReg = RspOp->base;
            info->SourceReg1 = UNUSED_OPERAND;
            info->flags = Load_Operation | VEC_Instruction;
            break;

        case RSP_LSC2_TV:
            info->flags = InvalidOpcode;
            break;
        default:
            CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
            info->flags = InvalidOpcode;
            break;
        }
        break;
    case RSP_SC2:
        switch (RspOp->rd)
        {
        case RSP_LSC2_BV:
        case RSP_LSC2_SV:
        case RSP_LSC2_LV:
        case RSP_LSC2_DV:
        case RSP_LSC2_QV:
        case RSP_LSC2_RV:
        case RSP_LSC2_PV:
        case RSP_LSC2_UV:
        case RSP_LSC2_HV:
        case RSP_LSC2_FV:
        case RSP_LSC2_WV:
            info->DestReg = RspOp->rt;
            info->IndexReg = RspOp->base;
            info->SourceReg1 = UNUSED_OPERAND;
            info->flags = Store_Operation | VEC_Instruction;
            break;
        case RSP_LSC2_TV:
            info->flags = InvalidOpcode;
            break;
        default:
            CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s", RSPInstruction(PC, RspOp->Value).NameAndParam().c_str()).c_str());
            info->flags = InvalidOpcode;
            break;
        }
        break;
    default:
        /*	CompilerWarning(stdstr_f("Unknown opcode in GetInstructionInfo\n%s",RSPOpcodeName(RspOp->Hex,PC)).c_str());
	*/
        info->flags = InvalidOpcode;
        break;
    }
}

/*
DelaySlotAffectBranch
Output:
True: Delay slot does affect the branch
False: Registers do not affect each other
Input: PC
*/

bool DelaySlotAffectBranch(uint32_t PC)
{
    uint32_t DelayPC = (PC + 4) & 0xFFC;
    if (IsOpcodeNop(DelayPC) == true)
    {
        return false;
    }

    RSPOpcode BranchOp, DelayOp;
    RSP_LW_IMEM(PC, &BranchOp.Value);
    RSP_LW_IMEM(DelayPC, &DelayOp.Value);

    OPCODE_INFO infoBranch, infoDelay;
    memset(&infoDelay, 0, sizeof(infoDelay));
    memset(&infoBranch, 0, sizeof(infoBranch));

    GetInstructionInfo(PC, &BranchOp, &infoBranch);
    GetInstructionInfo(DelayPC, &DelayOp, &infoDelay);

    if ((infoDelay.flags & COPO_MF_Instruction) == COPO_MF_Instruction)
    {
        return true;
    }

    if ((infoDelay.flags & Instruction_Mask) == VEC_Instruction)
    {
        return false;
    }

    if (infoBranch.SourceReg0 == infoDelay.DestReg)
    {
        return true;
    }
    if (infoBranch.SourceReg1 == infoDelay.DestReg)
    {
        return true;
    }

    return false;
}

/*
CompareInstructions
Output:
true: The opcodes are fine, no dependency
false: Watch it, these ops cant be touched
Input: Top, not the current operation, the one above
Bottom: The current opcode for re-ordering bubble style
*/

bool CompareInstructions(uint32_t PC, RSPOpcode * Top, RSPOpcode * Bottom)
{
    OPCODE_INFO info0, info1;
    uint32_t InstructionType;

    GetInstructionInfo(PC - 4, Top, &info0);
    GetInstructionInfo(PC, Bottom, &info1);

#ifdef COMPARE_INSTRUCTIONS_VERBOSE
    CPU_Message("Comparing %s (%X)", RSPOpcodeName(Top->Hex, PC - 4), PC - 4);
    CPU_Message("to %s (%X)", RSPOpcodeName(Bottom->Hex, PC), PC);
#endif

    // Usually branches and such
    if ((info0.flags & InvalidOpcode) != 0) return false;
    if ((info1.flags & InvalidOpcode) != 0) return false;

    if ((info0.flags & Flag_Instruction) != 0 && (info1.flags & Flag_Instruction) != 0) return false;

    InstructionType = (info0.flags & Instruction_Mask) << 2;
    InstructionType |= info1.flags & Instruction_Mask;
    InstructionType &= 0x0F; // Paranoia

    // 4-bit range, 16 possible combinations
    switch (InstructionType)
    {

    // Detect NOOP instruction, 7 cases, (see flags)
    case 0x01:
    case 0x02:
    case 0x03: // First is a NOOP
        return true;
    case 0x00: // Both?
    case 0x10:
    case 0x20:
    case 0x30: // Second is a NOOP
        return false;

    case 0x06: // GPR then Vector - 01,10
        if ((info0.flags & MemOperation_Mask) != 0 && (info1.flags & MemOperation_Mask) != 0)
        {
            // TODO: We have a vector and GPR memory operation
            return false;
        }
        else if ((info1.flags & MemOperation_Mask) != 0)
        {
            // We have a vector memory operation
            return (info1.IndexReg == info0.DestReg) ? false : true;
        }

        // We could have memory or normal GPR instruction here
        // paired with some kind of vector operation

        return true;
    case 0x0A: // Vector then Vector - 10,10

        /*
		Check for vector store then vector multiply (VMULF)
		This basically gives preferences to putting stores
		as close to the finish of an operation as possible
		*/

        if ((info0.flags & Store_Operation) != 0 && (info1.flags & Accum_Operation) != 0 && !(info1.flags & VEC_Accumulate))
        {
            return false;
        }

        // Look for loads and than some kind of vector operation
        // that does no accumulating, there is no reason to reorder

        if ((info0.flags & Load_Operation) != 0 && (info1.flags & Accum_Operation) != 0 && !(info1.flags & VEC_Accumulate))
        {
            return false;
        }

        if ((info0.flags & MemOperation_Mask) != 0 && (info1.flags & MemOperation_Mask) != 0)
        {

            // TODO: This is a pain, it's best to leave it alone

            return false;
        }
        else if ((info1.flags & MemOperation_Mask) != 0)
        {
            // Remember stored REG and loaded REG are the same
            if (info0.DestReg == info1.DestReg)
            {
                return false;
            }

            if (info1.flags & Load_Operation)
            {
                if (info0.SourceReg0 == info1.DestReg)
                {
                    return false;
                }
                if (info0.SourceReg1 == info1.DestReg)
                {
                    return false;
                }
            }
            else if (info1.flags & Store_Operation)
            {
                // It can store source REGS
                return true;
            }

            return true;
        }
        else if ((info0.flags & MemOperation_Mask) != 0)
        {
            // Remember stored REG and loaded REG are the same
            if (info0.DestReg == info1.DestReg)
            {
                return false;
            }

            if (info0.flags & Load_Operation)
            {
                if (info1.SourceReg0 == info0.DestReg)
                {
                    return false;
                }
                if (info1.SourceReg1 == info0.DestReg)
                {
                    return false;
                }
            }
            else if (info0.flags & Store_Operation)
            {
                // It can store source REGS
                return true;
            }

            return true;
        }
        else if ((info0.flags & VEC_Accumulate) != 0)
        {

            /*
			Example:
			VMACF
			VMUDH or VMADH or VADD
			*/

            return false;
        }
        else if ((info1.flags & VEC_Accumulate) != 0)
        {

            /*
			Example:
			VMULF
			VMADH
			*/

            return false;
        }
        else
        {

            /*
			Example:
			VMULF or VADDC
			VADD or VMUDH
			*/

            return false;
        }
        break;

    case 0x09: // Vector then GPR - 10,01

        /*
		This is where the bias comes into play, otherwise
		we can sit here all day swapping these 2 types
		*/

        return false;

    case 0x05: // GPR then GPR - 01,01
    case 0x07: // GPR then COP2 - 01, 11
    case 0x0D: // COP2 then GPR - 11, 01
    case 0x0F: // COP2 then COP2 - 11, 11
        return false;

    case 0x0B: // Vector then COP2 - 10, 11
        if (info1.flags & Load_Operation)
        {
            // Move to COP2 (destination) from GPR (source)
            if (info1.DestReg == info0.DestReg)
            {
                return false;
            }
            if (info1.DestReg == info0.SourceReg0)
            {
                return false;
            }
            if (info1.DestReg == info0.SourceReg1)
            {
                return false;
            }
        }
        else if (info1.flags & Store_Operation)
        {
            // Move from COP2 (source) to GPR (destination)
            if (info1.SourceReg0 == info0.DestReg)
            {
                return false;
            }
            if (info1.SourceReg0 == info0.SourceReg0)
            {
                return false;
            }
            if (info1.SourceReg0 == info0.SourceReg1)
            {
                return false;
            }
        }
        else
        {
            CompilerWarning("Reorder: unhandled vector than COP2");
        }
        // We want vectors on top
        return false;

    case 0x0E: // COP2 then Vector - 11, 10
        if (info0.flags & Load_Operation)
        {
            // Move to COP2 (destination) from GPR (source)
            if (info0.DestReg == info1.DestReg)
            {
                return false;
            }
            if (info0.DestReg == info1.SourceReg0)
            {
                return false;
            }
            if (info0.DestReg == info1.SourceReg1)
            {
                return false;
            }
        }
        else if (info0.flags & Store_Operation)
        {
            // Move from COP2 (source) to GPR (destination)
            if (info0.SourceReg0 == info1.DestReg)
            {
                return false;
            }
            if (info0.SourceReg0 == info1.SourceReg0)
            {
                return false;
            }
            if (info0.SourceReg0 == info1.SourceReg1)
            {
                return false;
            }
            if (info0.DestReg == info1.SourceReg0)
            {
                return false;
            }
        }
        else
        {
            CompilerWarning("Reorder: unhandled COP2 than vector");
        }
        // We want this at the top
        return true;

    default:
        CompilerWarning(stdstr_f("Reorder: Unhandled instruction type: %i", InstructionType).c_str());
    }

    return false;
}
