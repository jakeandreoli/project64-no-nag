#pragma once
#include "RSPOpcode.h"
#include <stdint.h>
#include <string>

class RSPInstruction
{
public:
    RSPInstruction(uint32_t Address, uint32_t Instruction);

    const char * Name();
    const char * Param();
    std::string NameAndParam();

private:
    RSPInstruction(void);
    RSPInstruction(const RSPInstruction &);
    RSPInstruction & operator=(const RSPInstruction &);

    void DecodeName(void);
    void DecodeSpecialName(void);
    void DecodeRegImmName(void);
    void DecodeCop0Name(void);
    void DecodeCop2Name(void);
    void DecodeLSC2Name(const char LoadStoreIdent);

    static const char * ElementSpecifier(uint32_t Element);

    uint32_t m_Address;
    RSPOpcode m_Instruction;
    char m_Name[40];
    char m_Param[200];
};