#include "RSPInstruction.h"
#include "RSPRegisters.h"
#include <Common/StdString.h>

RSPInstruction::RSPInstruction(uint32_t Address, uint32_t Instruction) :
    m_Address(Address)
{
    m_Name[0] = '\0';
    m_Param[0] = '\0';
    m_Instruction.Value = Instruction;
}

const char * RSPInstruction::Name()
{
    if (m_Name[0] == '\0')
    {
        DecodeName();
    }
    return m_Name;
}

const char * RSPInstruction::Param()
{
    if (m_Param[0] == '\0')
    {
        DecodeName();
    }
    return m_Param;
}

std::string RSPInstruction::NameAndParam()
{
    return stdstr_f("%s %s", Name(), Param());
}

void RSPInstruction::DecodeName(void)
{
    switch (m_Instruction.op)
    {
    case RSP_SPECIAL:
        DecodeSpecialName();
        break;
    case RSP_REGIMM:
        DecodeRegImmName();
        break;
    case RSP_J:
        strcpy(m_Name, "J");
        sprintf(m_Param, "0x%04X", (m_Instruction.target << 2) & 0x1FFC);
        break;
    case RSP_JAL:
        strcpy(m_Name, "JAL");
        sprintf(m_Param, "0x%04X", (m_Instruction.target << 2) & 0x1FFC);
        break;
    case RSP_BEQ:
        if (m_Instruction.rs == 0 && m_Instruction.rt == 0)
        {
            strcpy(m_Name, "B");
            sprintf(m_Param, "0x%08X", (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        else if (m_Instruction.rs == 0 || m_Instruction.rt == 0)
        {
            strcpy(m_Name, "BEQZ");
            sprintf(m_Param, "%s, 0x%08X", GPR_Name(m_Instruction.rs == 0 ? m_Instruction.rt : m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        else
        {
            strcpy(m_Name, "BEQ");
            sprintf(m_Param, "%s, %s, 0x%08X", GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        break;
    case RSP_BNE:
        if ((m_Instruction.rs == 0) ^ (m_Instruction.rt == 0))
        {
            strcpy(m_Name, "BNEZ");
            sprintf(m_Param, "%s, 0x%08X", GPR_Name(m_Instruction.rs == 0 ? m_Instruction.rt : m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        else
        {
            strcpy(m_Name, "BNE");
            sprintf(m_Param, "%s, %s, 0x%08X", GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        break;
    case RSP_BLEZ:
        strcpy(m_Name, "BLEZ");
        sprintf(m_Param, "%s, 0x%08X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        break;
    case RSP_BGTZ:
        strcpy(m_Name, "BGTZ");
        sprintf(m_Param, "%s, 0x%08X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        break;
    case RSP_ADDI:
        strcpy(m_Name, "ADDI");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_ADDIU:
        strcpy(m_Name, "ADDIU");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_SLTI:
        strcpy(m_Name, "SLTI");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_SLTIU:
        strcpy(m_Name, "SLTIU");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_ANDI:
        strcpy(m_Name, "ANDI");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_ORI:
        strcpy(m_Name, "ORI");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_XORI:
        strcpy(m_Name, "XORI");
        sprintf(m_Param, "%s, %s, 0x%04X", GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs), m_Instruction.immediate);
        break;
    case RSP_LUI:
        strcpy(m_Name, "LUI");
        sprintf(m_Param, "%s, 0x%04X", GPR_Name(m_Instruction.rt), m_Instruction.immediate);
        break;
    case RSP_CP0:
        DecodeCop0Name();
        break;
    case RSP_CP2:
        DecodeCop2Name();
        break;
    case RSP_LB:
        strcpy(m_Name, "LB");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LH:
        strcpy(m_Name, "LH");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LW:
        strcpy(m_Name, "LW");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LBU:
        strcpy(m_Name, "LBU");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LHU:
        strcpy(m_Name, "LHU");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LWU:
        strcpy(m_Name, "LWU");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_SB:
        strcpy(m_Name, "SB");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_SH:
        strcpy(m_Name, "SH");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_SW:
        strcpy(m_Name, "SW");
        sprintf(m_Param, "%s, 0x%04X (%s)", GPR_Name(m_Instruction.rt), m_Instruction.offset, GPR_Name(m_Instruction.base));
        break;
    case RSP_LC2:
        DecodeLSC2Name('L');
        break;
    case RSP_SC2:
        DecodeLSC2Name('S');
        break;
    default:
        strcpy(m_Name, "UNKNOWN");
        sprintf(m_Param, "0x%08X", m_Instruction.Value);
    }
}

void RSPInstruction::DecodeSpecialName(void)
{
    switch (m_Instruction.funct)
    {
    case RSP_SPECIAL_SLL:
        if (m_Instruction.Value != 0)
        {
            strcpy(m_Name, "SLL");
            sprintf(m_Param, "%s, %s, 0x%X", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), m_Instruction.sa);
        }
        else
        {
            strcpy(m_Name, "NOP");
        }
        break;
    case RSP_SPECIAL_SRL:
        strcpy(m_Name, "SRL");
        sprintf(m_Param, "%s, %s, 0x%X", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), m_Instruction.sa);
        break;
    case RSP_SPECIAL_SRA:
        strcpy(m_Name, "SRA");
        sprintf(m_Param, "%s, %s, 0x%X", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), m_Instruction.sa);
        break;
    case RSP_SPECIAL_SLLV:
        strcpy(m_Name, "SLLV");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs));
        break;
    case RSP_SPECIAL_SRLV:
        strcpy(m_Name, "SRLV");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs));
        break;
    case RSP_SPECIAL_SRAV:
        strcpy(m_Name, "SRAV");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rt), GPR_Name(m_Instruction.rs));
        break;
    case RSP_SPECIAL_JR:
        strcpy(m_Name, "JR");
        sprintf(m_Param, "%s", GPR_Name(m_Instruction.rs));
        break;
    case RSP_SPECIAL_JALR:
        strcpy(m_Name, "JALR");
        sprintf(m_Param, "%s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs));
        break;
    case RSP_SPECIAL_BREAK:
        strcpy(m_Name, "BREAK");
        break;
    case RSP_SPECIAL_ADD:
        strcpy(m_Name, "ADD");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_ADDU:
        strcpy(m_Name, "ADDU");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_SUB:
        strcpy(m_Name, "SUB");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_SUBU:
        strcpy(m_Name, "SUBU");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_AND:
        strcpy(m_Name, "AND");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_OR:
        strcpy(m_Name, "OR");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_XOR:
        strcpy(m_Name, "XOR");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_NOR:
        strcpy(m_Name, "NOR");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_SLT:
        strcpy(m_Name, "SLT");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    case RSP_SPECIAL_SLTU:
        strcpy(m_Name, "SLTU");
        sprintf(m_Param, "%s, %s, %s", GPR_Name(m_Instruction.rd), GPR_Name(m_Instruction.rs), GPR_Name(m_Instruction.rt));
        break;
    default:
        strcpy(m_Name, "UNKNOWN");
        sprintf(m_Param, "0x%08X", m_Instruction.Value);
    }
}

void RSPInstruction::DecodeRegImmName(void)
{
    switch (m_Instruction.rt)
    {
    case RSP_REGIMM_BLTZ:
        strcpy(m_Name, "BLTZ");
        sprintf(m_Param, "%s, 0x%04X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        break;
    case RSP_REGIMM_BGEZ:
        if (m_Instruction.rs == 0)
        {
            strcpy(m_Name, "B");
            sprintf(m_Param, "0x%04X", (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        else
        {
            strcpy(m_Name, "BGEZ");
            sprintf(m_Param, "%s, 0x%04X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        break;
    case RSP_REGIMM_BLTZAL:
        strcpy(m_Name, "BLTZAL");
        sprintf(m_Param, "%s, 0x%04X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        break;
    case RSP_REGIMM_BGEZAL:
        if (m_Instruction.rs == 0)
        {
            strcpy(m_Name, "BAL");
            sprintf(m_Param, "0x%04X", (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        else
        {
            strcpy(m_Name, "BGEZAL");
            sprintf(m_Param, "%s, 0x%04X", GPR_Name(m_Instruction.rs), (m_Address + ((short)m_Instruction.offset << 2) + 4) & 0x1FFC);
        }
        break;
    default:
        strcpy(m_Name, "UNKNOWN");
        sprintf(m_Param, "0x%08X", m_Instruction.Value);
    }
}

void RSPInstruction::DecodeCop0Name(void)
{
    switch (m_Instruction.rs)
    {
    case RSP_COP0_MF:
        strcpy(m_Name, "MFC0");
        sprintf(m_Param, "%s, %s", GPR_Name(m_Instruction.rt), COP0_Name(m_Instruction.rd));
        break;
    case RSP_COP0_MT:
        strcpy(m_Name, "MTC0");
        sprintf(m_Param, "%s, %s", GPR_Name(m_Instruction.rt), COP0_Name(m_Instruction.rd));
        break;
    default:
        strcpy(m_Name, "UNKNOWN");
        sprintf(m_Param, "0x%08X", m_Instruction.Value);
    }
}

void RSPInstruction::DecodeCop2Name(void)
{
    if ((m_Instruction.rs & 0x10) == 0)
    {
        switch (m_Instruction.rs)
        {
        case RSP_COP2_MF:
            strcpy(m_Name, "MFC2");
            sprintf(m_Param, "%s, $v%d[%d]", GPR_Name(m_Instruction.rt), m_Instruction.rd, m_Instruction.sa >> 1);
            break;
        case RSP_COP2_CF:
            strcpy(m_Name, "CFC2");
            sprintf(m_Param, "%s, %d", GPR_Name(m_Instruction.rt), m_Instruction.rd % 4);
            break;
        case RSP_COP2_MT:
            strcpy(m_Name, "MTC2");
            sprintf(m_Param, "%s, $v%d[%d]", GPR_Name(m_Instruction.rt), m_Instruction.rd, m_Instruction.sa >> 1);
            break;
        case RSP_COP2_CT:
            strcpy(m_Name, "CTC2");
            sprintf(m_Param, "%s, %d", GPR_Name(m_Instruction.rt), m_Instruction.rd % 4);
            break;
        default:
            strcpy(m_Name, "UNKNOWN");
            sprintf(m_Param, "0x%08X", m_Instruction.Value);
        }
    }
    else
    {
        switch (m_Instruction.funct)
        {
        case RSP_VECTOR_VMULF:
            strcpy(m_Name, "VMULF");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMULU:
            strcpy(m_Name, "VMULU");
            sprintf(m_Param, "$v%d, %d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VRNDP:
            strcpy(m_Name, "VRNDP");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, (m_Instruction.vs & 1), m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMULQ:
            strcpy(m_Name, "VMULQ");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMUDL:
            strcpy(m_Name, "VMUDL");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMUDM:
            strcpy(m_Name, "VMUDM");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMUDN:
            strcpy(m_Name, "VMUDN");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMUDH:
            strcpy(m_Name, "VMUDH");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMACF:
            strcpy(m_Name, "VMACF");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMACU:
            strcpy(m_Name, "VMACU");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VRNDN:
            strcpy(m_Name, "VRNDN");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, (m_Instruction.vs & 1), m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMACQ:
            strcpy(m_Name, "VMACQ");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMADL:
            strcpy(m_Name, "VMADL");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMADM:
            strcpy(m_Name, "VMADM");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMADN:
            strcpy(m_Name, "VMADN");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMADH:
            strcpy(m_Name, "VMADH");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VADD:
            strcpy(m_Name, "VADD");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUB:
            strcpy(m_Name, "VSUB");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUT:
            strcpy(m_Name, "Reserved (VSUT)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VABS:
            strcpy(m_Name, "VABS");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VADDC:
            strcpy(m_Name, "VADDC");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUBC:
            strcpy(m_Name, "VSUBC");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VADDB:
            strcpy(m_Name, "Reserved (VADDB)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUBB:
            strcpy(m_Name, "Reserved (VSUBB)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VACCB:
            strcpy(m_Name, "Reserved (VACCB)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUCB:
            strcpy(m_Name, "Reserved (VSUCB)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSAD:
            strcpy(m_Name, "Reserved (VSAD)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSAC:
            strcpy(m_Name, "Reserved (VSAC)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUM:
            strcpy(m_Name, "Reserved (VSUM)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSAW:
            strcpy(m_Name, "VSAW");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VACC:
            strcpy(m_Name, "Reserved (VACC)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VSUC:
            strcpy(m_Name, "Reserved (VSUC)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VLT:
            strcpy(m_Name, "VLT");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VEQ:
            strcpy(m_Name, "VEQ");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VNE:
            strcpy(m_Name, "VNE");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VGE:
            strcpy(m_Name, "VGE");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VCL:
            strcpy(m_Name, "VCL");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VCH:
            strcpy(m_Name, "VCH");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VCR:
            strcpy(m_Name, "VCR");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VMRG:
            strcpy(m_Name, "VMRG");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VAND:
            strcpy(m_Name, "VAND");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VNAND:
            strcpy(m_Name, "VNAND");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VOR:
            strcpy(m_Name, "VOR");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VNOR:
            strcpy(m_Name, "VNOR");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VXOR:
            strcpy(m_Name, "VXOR");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VNXOR:
            strcpy(m_Name, "VNXOR");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_V056:
            strcpy(m_Name, "Reserved (V056)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_V057:
            strcpy(m_Name, "Reserved (V057)");
            sprintf(m_Param, "$v%d, $v%d, $v%d%s", m_Instruction.vd, m_Instruction.vs, m_Instruction.vt, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VRCP:
            strcpy(m_Name, "VRCP");
            sprintf(m_Param, "$v%d[%d], $v%d [%d]", m_Instruction.vd, m_Instruction.rd & 0x7, m_Instruction.vt, m_Instruction.e & 0x7);
            break;
        case RSP_VECTOR_VRCPL:
            strcpy(m_Name, "VRCPL");
            sprintf(m_Param, "$v%d[%d], $v%d [%d]", m_Instruction.vd, m_Instruction.rd & 0x7, m_Instruction.vt, m_Instruction.e & 0x7);
            break;
        case RSP_VECTOR_VRCPH:
            strcpy(m_Name, "VRCPH");
            sprintf(m_Param, "$v%d[%d], $v%d%s[%d]", m_Instruction.vd, m_Instruction.de & 0x7, m_Instruction.vt, ElementSpecifier(m_Instruction.e), (m_Instruction.de & 0x7));
            break;
        case RSP_VECTOR_VMOV:
            strcpy(m_Name, "VMOV");
            sprintf(m_Param, "$v%d[%d], $v%d[%d]%s", m_Instruction.vd, m_Instruction.de & 0x7, m_Instruction.rt, m_Instruction.de & 0x7, ElementSpecifier(m_Instruction.e));
            break;
        case RSP_VECTOR_VRSQ:
            strcpy(m_Name, "VRSQ");
            sprintf(m_Param, "$v%d[%d], $v%d [%d]", m_Instruction.vd, m_Instruction.rd & 0x7, m_Instruction.vt, m_Instruction.e & 0x7);
            break;
        case RSP_VECTOR_VRSQL:
            strcpy(m_Name, "VRSQL");
            sprintf(m_Param, "$v%d[%d], $v%d [%d]", m_Instruction.vd, m_Instruction.rd & 0x7, m_Instruction.vt, m_Instruction.e & 0x7);
            break;
        case RSP_VECTOR_VRSQH:
            strcpy(m_Name, "VRSQH");
            sprintf(m_Param, "$v%d[%d], $v%d%s[%d]", m_Instruction.vd, m_Instruction.rd & 0x7, m_Instruction.vt, ElementSpecifier(m_Instruction.e), (m_Instruction.rd & 0x7));
            break;
        case RSP_VECTOR_VNOP:
            strcpy(m_Name, "VNOP");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VEXTT:
            strcpy(m_Name, "Reserved (VEXTT)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VEXTQ:
            strcpy(m_Name, "Reserved (VEXTQ)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VEXTN:
            strcpy(m_Name, "Reserved (VEXTN)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_V073:
            strcpy(m_Name, "Reserved (V073)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VINST:
            strcpy(m_Name, "Reserved (VINST)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VINSQ:
            strcpy(m_Name, "Reserved (VINSQ)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VINSN:
            strcpy(m_Name, "Reserved (VINSN)");
            strcpy(m_Param, "");
            break;
        case RSP_VECTOR_VNULL:
            strcpy(m_Name, "VNULL");
            strcpy(m_Param, "");
            break;
        default:
            strcpy(m_Name, "UNKNOWN");
            sprintf(m_Param, "0x%08X", m_Instruction.Value);
        }
    }
}

void RSPInstruction::DecodeLSC2Name(const char LoadStoreIdent)
{
    switch (m_Instruction.rd)
    {
    case RSP_LSC2_BV:
        sprintf(m_Name, "%cBV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 0), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_SV:
        sprintf(m_Name, "%cSV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 1), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_LV:
        sprintf(m_Name, "%cLV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 2), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_DV:
        sprintf(m_Name, "%cDV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 3), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_QV:
        sprintf(m_Name, "%cQV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_RV:
        sprintf(m_Name, "%cRV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_PV:
        sprintf(m_Name, "%cPV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 3), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_UV:
        sprintf(m_Name, "%cUV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 3), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_HV:
        sprintf(m_Name, "%cHV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_FV:
        sprintf(m_Name, "%cFV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_WV:
        sprintf(m_Name, "%cWV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    case RSP_LSC2_TV:
        sprintf(m_Name, "%cTV", LoadStoreIdent);
        sprintf(m_Param, "$v%d[%d], %s0x%03X(%s)", m_Instruction.rt, m_Instruction.del, (m_Instruction.voffset < 0) ? "-" : "", abs(m_Instruction.voffset << 4), GPR_Name(m_Instruction.base));
        break;
    default:
        strcpy(m_Name, "UNKNOWN");
        sprintf(m_Param, "0x%08X", m_Instruction.Value);
    }
}

const char * RSPInstruction::ElementSpecifier(uint32_t Element)
{
    switch (Element)
    {
    case 0: return "";
    case 1: return "";
    case 2: return " [0q]";
    case 3: return " [1q]";
    case 4: return " [0h]";
    case 5: return " [1h]";
    case 6: return " [2h]";
    case 7: return " [3h]";
    case 8: return " [0]";
    case 9: return " [1]";
    case 10: return " [2]";
    case 11: return " [3]";
    case 12: return " [4]";
    case 13: return " [5]";
    case 14: return " [6]";
    case 15: return " [7]";
    }
    return "Unknown Element";
}
