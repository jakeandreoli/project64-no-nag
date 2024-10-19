#pragma once
#include "R4300iOpcode.h"
#include <string>

class R4300iInstruction
{
public:
    R4300iInstruction(uint64_t Address, uint32_t Instruction);
    R4300iInstruction & operator=(const R4300iInstruction &);

    const uint64_t & Address() const;
    const R4300iOpcode & Opcode() const;
    const char * Name();
    const char * Param();
    std::string NameAndParam();

    bool HasDelaySlot(void) const;
    bool DelaySlotEffectsCompare(uint32_t DelayInstruction) const;
    void ReadsGPR(uint32_t & Reg1, uint32_t & Reg2) const;
    uint32_t WritesGPR(void) const;
    bool ReadsHI() const;
    bool ReadsLO() const;
    bool WritesHI() const;
    bool WritesLO() const;

private:
    R4300iInstruction(void);
    R4300iInstruction(const R4300iInstruction &);

    static const char * FPR_Type(uint32_t COP1OpCode);

    void DecodeName(void);
    void DecodeSpecialName(void);
    void DecodeRegImmName(void);
    void DecodeCop1Name(void);
    void DecodeCop2Name(void);

    uint64_t m_Address;
    R4300iOpcode m_Instruction;
    char m_Name[40];
    char m_Param[200];
};