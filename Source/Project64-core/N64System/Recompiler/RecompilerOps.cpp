#include "stdafx.h"

#include "RecompilerOps.h"
#include <Project64-core/N64System/N64System.h>

CRecompilerOpsBase::CRecompilerOpsBase(CN64System & System, CCodeBlock & CodeBlock) :
    m_System(System),
    m_SystemEvents(System.m_SystemEvents),
    m_Reg(System.m_Reg),
    m_TLB(System.m_TLB),
    m_MMU(System.m_MMU_VM),
    m_CodeBlock(CodeBlock),
    m_Instruction(0, 0),
    m_Opcode(m_Instruction.Opcode()),
    m_Section(nullptr)
{
}

CRecompilerOpsBase::~CRecompilerOpsBase()
{
}
