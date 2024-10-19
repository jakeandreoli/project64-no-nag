#include "stdafx.h"

#include <Common\MemoryManagement.h>
#include <Project64-core\Debugger.h>
#include <Project64-core\N64System\Mips\Disk.h>
#include <Project64-core\N64System\Mips\MemoryVirtualMem.h>
#include <Project64-core\N64System\N64Rom.h>
#include <Project64-core\N64System\N64System.h>
#include <Project64-core\N64System\SystemGlobals.h>
#include <stdio.h>

uint32_t CMipsMemoryVM::RegModValue;

#pragma warning(disable : 4355) // Disable 'this' : used in base member initializer list

CMipsMemoryVM::CMipsMemoryVM(CN64System & System, bool SavesReadOnly) :
    m_System(System),
    m_Reg(System.m_Reg),
    m_TLB(System.m_TLB),
    m_AudioInterfaceHandler(System, System.m_Reg),
    m_CartridgeDomain1Address1Handler(System.m_Reg, g_DDRom),
    m_CartridgeDomain2Address1Handler(System.m_Reg),
    m_CartridgeDomain2Address2Handler(System, System.m_Reg, *this, SavesReadOnly),
    m_RDRAMRegistersHandler(System.m_Reg),
    m_DPCommandRegistersHandler(System, System.GetPlugins(), System.m_Reg),
    m_ISViewerHandler(System, m_RomMemoryHandler, *g_Rom),
    m_MIPSInterfaceHandler(System.m_Reg),
    m_PeripheralInterfaceHandler(System, *this, System.m_Reg, m_CartridgeDomain2Address2Handler),
    m_PifRamHandler(System, SavesReadOnly),
    m_RDRAMInterfaceHandler(System.m_Reg),
    m_RomMemoryHandler(System, System.m_Reg, *g_Rom),
    m_SerialInterfaceHandler(*this, System.m_Reg),
    m_SPRegistersHandler(System, *this, System.m_Reg),
    m_VideoInterfaceHandler(System, *this, System.m_Reg),
    m_MemoryReadMap(nullptr),
    m_MemoryWriteMap(nullptr),
    m_TLB_ReadMap(nullptr),
    m_TLB_WriteMap(nullptr),
    m_RDRAM(nullptr),
    m_Rom(*g_Rom)
{
    g_Settings->RegisterChangeCB(Game_RDRamSize, this, (CSettings::SettingChangedFunc)RdramChanged);
}

CMipsMemoryVM::~CMipsMemoryVM()
{
    g_Settings->UnregisterChangeCB(Game_RDRamSize, this, (CSettings::SettingChangedFunc)RdramChanged);
    FreeMemory();
}

void CMipsMemoryVM::Reset(bool /*EraseMemory*/)
{
    if (m_MemoryReadMap != nullptr && m_MemoryWriteMap != nullptr)
    {
        memset(m_MemoryReadMap, -1, 0x100000 * sizeof(m_MemoryReadMap[0]));
        memset(m_MemoryWriteMap, -1, 0x100000 * sizeof(m_MemoryWriteMap[0]));
        for (uint32_t i = 0; i < 2; i++)
        {
            uint32_t BaseAddress = i == 0 ? 0x80000000 : 0xA0000000;
            for (size_t Address = BaseAddress; Address < BaseAddress + m_AllocatedRdramSize; Address += 0x1000)
            {
                m_MemoryReadMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
                m_MemoryWriteMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
            }
            for (size_t Address = BaseAddress + 0x04000000; Address < (BaseAddress + 0x04001000); Address += 0x1000)
            {
                m_MemoryReadMap[Address >> 12] = (size_t)(Dmem() - Address);
            }
            for (size_t Address = BaseAddress + 0x04001000; Address < (BaseAddress + 0x04002000); Address += 0x1000)
            {
                m_MemoryReadMap[Address >> 12] = (size_t)(Imem() - Address);
            }
        }
    }
    if (m_TLB_ReadMap)
    {
        memset(m_TLB_ReadMap, -1, 0xFFFFF * sizeof(m_TLB_ReadMap[0]));
        memset(m_TLB_WriteMap, -1, 0xFFFFF * sizeof(m_TLB_WriteMap[0]));
        for (uint32_t Address = 0x80000000; Address < 0xC0000000; Address += 0x1000)
        {
            m_TLB_ReadMap[Address >> 12] = (Address & 0x1FFFFFFF) - Address;
            m_TLB_WriteMap[Address >> 12] = (Address & 0x1FFFFFFF) - Address;
        }

        if (g_Settings->LoadDword(Rdb_TLB_VAddrStart) != 0)
        {
            uint32_t Start = g_Settings->LoadDword(Rdb_TLB_VAddrStart); //0x7F000000;
            uint32_t Len = g_Settings->LoadDword(Rdb_TLB_VAddrLen);     //0x01000000;
            uint32_t PAddr = g_Settings->LoadDword(Rdb_TLB_PAddrStart); //0x10034b30;
            uint32_t End = Start + Len;
            for (uint32_t Address = Start; Address < End; Address += 0x1000)
            {
                uint32_t TargetAddress = (Address - Start + PAddr);
                if (TargetAddress < m_AllocatedRdramSize)
                {
                    m_MemoryReadMap[Address >> 12] = ((size_t)m_RDRAM + TargetAddress) - Address;
                    m_MemoryWriteMap[Address >> 12] = ((size_t)m_RDRAM + TargetAddress) - Address;
                }
                if (TargetAddress >= 0x10000000 && TargetAddress < (0x10000000 + g_Rom->GetRomSize()))
                {
                    m_MemoryReadMap[Address >> 12] = ((size_t)g_Rom->GetRomAddress() + (TargetAddress - 0x10000000)) - Address;
                }
                m_TLB_ReadMap[Address >> 12] = TargetAddress - Address;
                m_TLB_WriteMap[Address >> 12] = TargetAddress - Address;
            }
        }
    }
}

bool CMipsMemoryVM::Initialize(void)
{
    if (m_RDRAM != nullptr)
    {
        return true;
    }
    m_RDRAM = (uint8_t *)AllocateAddressSpace(0x02000000, (void *)g_Settings->LoadDword(Setting_FixedRdramAddress));
    if (m_RDRAM == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to reserve RDRAM (Size: 0x%X)", 0x02000000);
        FreeMemory();
        return false;
    }

    bool RdbRamSet = g_Settings->LoadDword(Rdb_RDRamSize, m_AllocatedRdramSize);
    if (!g_Settings->LoadDword(Game_RDRamSize, m_AllocatedRdramSize) && !RdbRamSet)
    {
        m_AllocatedRdramSize = g_Settings->LoadDword(g_Settings->LoadBool(Game_Known) ? Default_RDRamSizeKnown : Default_RDRamSizeUnknown);
    }
    if (CommitMemory(m_RDRAM, m_AllocatedRdramSize, MEM_READWRITE) == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to allocate RDRAM (Size: 0x%X)", m_AllocatedRdramSize);
        FreeMemory();
        return false;
    }
    g_Settings->SaveDword(Setting_AllocatedRdramSize, m_AllocatedRdramSize);

    m_MemoryReadMap = new size_t[0x100000];
    if (m_MemoryReadMap == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to allocate m_MemoryReadMap (Size: 0x%X)", 0x100000 * sizeof(size_t));
        FreeMemory();
        return false;
    }
    m_MemoryWriteMap = new size_t[0x100000];
    if (m_MemoryWriteMap == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to allocate m_MemoryWriteMap (Size: 0x%X)", 0x100000 * sizeof(size_t));
        FreeMemory();
        return false;
    }

    m_TLB_ReadMap = new uint32_t[0x100000];
    if (m_TLB_ReadMap == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to allocate m_TLB_ReadMap (Size: 0x%X)", 0x100000 * sizeof(size_t));
        FreeMemory();
        return false;
    }

    m_TLB_WriteMap = new uint32_t[0x100000];
    if (m_TLB_WriteMap == nullptr)
    {
        WriteTrace(TraceN64System, TraceError, "Failed to allocate m_TLB_WriteMap (Size: 0x%X)", 0xFFFFF * sizeof(size_t));
        FreeMemory();
        return false;
    }
    Reset(false);
    return true;
}

void CMipsMemoryVM::FreeMemory()
{
    if (m_RDRAM)
    {
        DecommitMemory(m_RDRAM, 0x02000000);
        FreeAddressSpace(m_RDRAM, 0x02000000);
        m_RDRAM = nullptr;
    }
    if (m_TLB_ReadMap)
    {
        delete[] m_TLB_ReadMap;
        m_TLB_ReadMap = nullptr;
    }
    if (m_TLB_WriteMap)
    {
        delete[] m_TLB_WriteMap;
        m_TLB_WriteMap = nullptr;
    }
    if (m_MemoryReadMap)
    {
        delete[] m_MemoryReadMap;
        m_MemoryReadMap = nullptr;
    }
    if (m_MemoryWriteMap)
    {
        delete[] m_MemoryWriteMap;
        m_MemoryWriteMap = nullptr;
    }
}

uint8_t * CMipsMemoryVM::MemoryPtr(uint32_t VAddr, uint32_t Size, bool Read)
{
    if (m_TLB_ReadMap[VAddr >> 12] == -1)
    {
        return nullptr;
    }
    uint32_t PAddr = m_TLB_ReadMap[VAddr >> 12] + VAddr;
    if ((PAddr + Size) < m_AllocatedRdramSize)
    {
        return (uint8_t *)(m_RDRAM + PAddr);
    }

    if (PAddr >= 0x04000000 && (PAddr + Size) <= 0x04001000)
    {
        return (uint8_t *)&m_SPRegistersHandler.Dmem()[PAddr - 0x04000000];
    }
    if (PAddr >= 0x04001000 && (PAddr + Size) <= 0x04002000)
    {
        return (uint8_t *)&m_SPRegistersHandler.Imem()[PAddr - 0x04001000];
    }
    if (Read && PAddr >= 0x10000000 && (PAddr + Size) < (0x10000000 + m_Rom.GetRomSize()))
    {
        return (uint8_t *)&m_Rom.GetRomAddress()[PAddr - 0x10000000];
    }

    g_Notify->BreakPoint(__FILE__, __LINE__);
    return nullptr;
}

bool CMipsMemoryVM::MemoryValue8(uint32_t VAddr, uint8_t & Value)
{
    uint8_t * ptr = MemoryPtr(VAddr ^ 3, 1, true);
    if (ptr == nullptr)
    {
        return false;
    }
    Value = *ptr;
    return true;
}

bool CMipsMemoryVM::MemoryValue16(uint32_t VAddr, uint16_t & Value)
{
    uint8_t * ptr = MemoryPtr(VAddr ^ 2, 2, true);
    if (ptr == nullptr)
    {
        return false;
    }
    Value = *(uint16_t *)(ptr);
    return true;
}

bool CMipsMemoryVM::MemoryValue32(uint32_t VAddr, uint32_t & Value)
{
    uint8_t * ptr = MemoryPtr(VAddr, 4, true);
    if (ptr == nullptr)
    {
        return false;
    }
    Value = *(uint32_t *)(ptr);
    return true;
}

bool CMipsMemoryVM::MemoryValue64(uint32_t VAddr, uint64_t & Value)
{
    uint8_t * ptr = MemoryPtr(VAddr, 8, true);
    if (ptr == nullptr)
    {
        return false;
    }
    *((uint32_t *)(&Value) + 1) = *(uint32_t *)(ptr);
    *((uint32_t *)(&Value) + 0) = *(uint32_t *)(ptr + 4);
    return true;
}

bool CMipsMemoryVM::UpdateMemoryValue8(uint32_t VAddr, uint8_t Value)
{
    uint8_t * ptr = MemoryPtr(VAddr ^ 3, 1, false);
    if (ptr == nullptr)
    {
        return false;
    }
    *ptr = Value;
    return true;
}

bool CMipsMemoryVM::UpdateMemoryValue16(uint32_t VAddr, uint16_t Value)
{
    uint8_t * ptr = MemoryPtr(VAddr ^ 2, 2, false);
    if (ptr == nullptr)
    {
        return false;
    }
    *(uint16_t *)(ptr) = Value;
    return true;
}

bool CMipsMemoryVM::UpdateMemoryValue32(uint32_t VAddr, uint32_t Value)
{
    uint8_t * ptr = MemoryPtr(VAddr, 4, false);
    if (ptr == nullptr)
    {
        return false;
    }
    *(uint32_t *)(ptr) = Value;
    return true;
}

bool CMipsMemoryVM::LB_Memory(uint64_t VAddr, uint8_t & Value)
{
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_RADE : EXC_RMISS);
            return false;
        }
        return LB_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveReadBP() && g_Debugger->ReadBP8(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryReadMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        Value = *(uint8_t *)(MemoryPtr + (VAddr32 ^ 3));
        return true;
    }
    return LB_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::LH_Memory(uint64_t VAddr, uint16_t & Value)
{
    if ((VAddr & 1) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_RADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_RADE : EXC_RMISS);
            return false;
        }
        return LH_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveReadBP() && g_Debugger->ReadBP16(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryReadMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        Value = *(uint16_t *)(MemoryPtr + (VAddr32 ^ 2));
        return true;
    }
    return LH_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::LW_Memory(uint64_t VAddr, uint32_t & Value)
{
    if ((VAddr & 3) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_RADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_RADE : EXC_RMISS);
            return false;
        }
        return LW_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveReadBP() && g_Debugger->ReadBP32(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryReadMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        Value = *(uint32_t *)(MemoryPtr + VAddr32);
        return true;
    }
    return LW_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::LD_Memory(uint64_t VAddr, uint64_t & Value)
{
    if ((VAddr & 7) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_RADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_RADE : EXC_RMISS);
            return false;
        }
        return LD_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveReadBP() && g_Debugger->ReadBP64(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryReadMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        *((uint32_t *)(&Value) + 1) = *(uint32_t *)(MemoryPtr + VAddr32);
        *((uint32_t *)(&Value) + 0) = *(uint32_t *)(MemoryPtr + VAddr32 + 4);
        return true;
    }
    return LD_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::SB_Memory(uint64_t VAddr, uint32_t Value)
{
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_WADE : EXC_WMISS);
            return false;
        }
        return SB_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveWriteBP() && g_Debugger->WriteBP8(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryWriteMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        *(uint8_t *)(MemoryPtr + (VAddr32 ^ 3)) = (uint8_t)Value;
        return true;
    }
    return SB_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::SH_Memory(uint64_t VAddr, uint32_t Value)
{
    if ((VAddr & 1) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_WADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_WADE : EXC_WMISS);
            return false;
        }
        return SH_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveWriteBP() && g_Debugger->WriteBP16(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryWriteMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        *(uint16_t *)(MemoryPtr + (VAddr32 ^ 2)) = (uint16_t)Value;
        return true;
    }
    return SH_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::SW_Memory(uint64_t VAddr, uint32_t Value)
{
    if ((VAddr & 3) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_WADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_WADE : EXC_WMISS);
            return false;
        }
        return SW_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveWriteBP() && g_Debugger->WriteBP32(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryWriteMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        *(uint32_t *)(MemoryPtr + VAddr32) = Value;
        return true;
    }
    return SW_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::SD_Memory(uint64_t VAddr, uint64_t Value)
{
    if ((VAddr & 7) != 0)
    {
        m_Reg.TriggerAddressException(VAddr, EXC_WADE);
        return false;
    }
    if ((uint64_t)((int32_t)VAddr) != VAddr)
    {
        uint32_t PAddr;
        bool MemoryUnused;
        if (!m_TLB.VAddrToPAddr(VAddr, PAddr, MemoryUnused))
        {
            m_Reg.TriggerAddressException(VAddr, MemoryUnused ? EXC_WADE : EXC_WMISS);
            return false;
        }
        return SD_PhysicalAddress(PAddr, Value);
    }
    uint32_t VAddr32 = (uint32_t)VAddr;
    if (HaveWriteBP() && g_Debugger->WriteBP64(VAddr32) && MemoryBreakpoint())
    {
        return false;
    }
    uint8_t * MemoryPtr = (uint8_t *)m_MemoryWriteMap[VAddr32 >> 12];
    if (MemoryPtr != (uint8_t *)-1)
    {
        *(uint32_t *)(MemoryPtr + VAddr32 + 0) = *((uint32_t *)(&Value) + 1);
        *(uint32_t *)(MemoryPtr + VAddr32 + 4) = *((uint32_t *)(&Value));
        return true;
    }
    return SD_VAddr32(VAddr32, Value);
}

bool CMipsMemoryVM::ValidVaddr(uint32_t VAddr) const
{
    return m_TLB_ReadMap[VAddr >> 12] != -1;
}

bool CMipsMemoryVM::VAddrToPAddr(uint32_t VAddr, uint32_t & PAddr) const
{
    if (m_TLB_ReadMap[VAddr >> 12] == -1)
    {
        return false;
    }
    PAddr = m_TLB_ReadMap[VAddr >> 12] + VAddr;
    return true;
}

bool CMipsMemoryVM::MemoryBreakpoint()
{
    if (g_Settings->LoadBool(Debugger_SteppingOps))
    {
        return false;
    }
    g_Settings->SaveBool(Debugger_SteppingOps, true);
    g_Debugger->WaitForStep();
    if (SkipOp())
    {
        // Skip command if instructed by the debugger
        g_Settings->SaveBool(Debugger_SkipOp, false);
        return true;
    }
    return false;
}

bool CMipsMemoryVM::LB_VAddr32(uint32_t VAddr, uint8_t & Value)
{
    uint32_t BaseAddress = m_TLB_ReadMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_RMISS);
        return false;
    }
    return LB_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::LH_VAddr32(uint32_t VAddr, uint16_t & Value)
{
    uint32_t BaseAddress = m_TLB_ReadMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_RMISS);
        return false;
    }
    return LH_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::LW_VAddr32(uint32_t VAddr, uint32_t & Value)
{
    uint32_t BaseAddress = m_TLB_ReadMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_RMISS);
        return false;
    }
    return LW_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::LD_VAddr32(uint32_t VAddr, uint64_t & Value)
{
    uint32_t BaseAddress = m_TLB_ReadMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_RMISS);
        return false;
    }
    return LD_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::LB_PhysicalAddress(uint32_t PAddr, uint8_t & Value)
{
    uint32_t ReadAddress = PAddr & ~3;
    uint32_t Value32;
    switch (PAddr & 0xFFF00000)
    {
    case 0x1FC00000: m_PifRamHandler.Read32(ReadAddress, Value32); break;
    default:
        if (PAddr < RdramSize())
        {
            Value = *(uint8_t *)(m_RDRAM + (PAddr ^ 3));
            return true;
        }
        else if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            if (!m_RomMemoryHandler.Read32(PAddr, Value32))
            {
                return false;
            }
            Value = ((Value32 >> (((PAddr & 1) ^ 3) << 3)) & 0xff);
            return true;
        }
        else
        {
            if (BreakOnUnhandledMemory())
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
            Value32 = 0;
        }
    }
    Value = ((Value32 >> (((PAddr & 3) ^ 3) << 3)) & 0xff);
    return true;
}

bool CMipsMemoryVM::LH_PhysicalAddress(uint32_t PAddr, uint16_t & Value)
{
    uint32_t ReadAddress = PAddr & ~1;
    uint32_t Value32;
    switch (PAddr & 0xFFF00000)
    {
    case 0x1FC00000: m_PifRamHandler.Read32(ReadAddress, Value32); break;
    default:
        if (PAddr < RdramSize())
        {
            Value = *(uint16_t *)(m_RDRAM + (PAddr ^ 2));
            return true;
        }
        else if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            if (!m_RomMemoryHandler.Read32(PAddr, Value32))
            {
                return false;
            }
            Value = ((Value32 >> 16) & 0xffff);
            return true;
        }
        else
        {
            if (BreakOnUnhandledMemory())
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
            Value32 = 0;
        }
    }
    Value = ((Value32 >> (((PAddr ^ 1) & 1) << 4)) & 0xffff);
    return true;
}

bool CMipsMemoryVM::LW_PhysicalAddress(uint32_t PAddr, uint32_t & Value)
{
    switch (PAddr & 0xFFF00000)
    {
    case 0x03F00000: m_RDRAMRegistersHandler.Read32(PAddr, Value); break;
    case 0x04000000: m_SPRegistersHandler.Read32(PAddr, Value); break;
    case 0x04100000: m_DPCommandRegistersHandler.Read32(PAddr, Value); break;
    case 0x04300000: m_MIPSInterfaceHandler.Read32(PAddr, Value); break;
    case 0x04400000: m_VideoInterfaceHandler.Read32(PAddr, Value); break;
    case 0x04500000: m_AudioInterfaceHandler.Read32(PAddr, Value); break;
    case 0x04600000: m_PeripheralInterfaceHandler.Read32(PAddr, Value); break;
    case 0x04700000: m_RDRAMInterfaceHandler.Read32(PAddr, Value); break;
    case 0x04800000: m_SerialInterfaceHandler.Read32(PAddr, Value); break;
    case 0x05000000: m_CartridgeDomain2Address1Handler.Read32(PAddr, Value); break;
    case 0x06000000: m_CartridgeDomain1Address1Handler.Read32(PAddr, Value); break;
    case 0x08000000: m_CartridgeDomain2Address2Handler.Read32(PAddr, Value); break;
    case 0x13F00000: m_ISViewerHandler.Read32(PAddr, Value); break;
    case 0x1FC00000: m_PifRamHandler.Read32(PAddr, Value); break;
    case 0x1FF00000: m_CartridgeDomain1Address3Handler.Read32(PAddr, Value); break;
    default:
        if (PAddr < RdramSize())
        {
            Value = *(uint32_t *)(m_RDRAM + PAddr);
        }
        else if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            m_RomMemoryHandler.Read32(PAddr, Value);
        }
        else if (BreakOnUnhandledMemory())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
            Value = ((PAddr & 0xFFFF) << 16) | (PAddr & 0xFFFF);
        }
        else
        {
            Value = ((PAddr & 0xFFFF) << 16) | (PAddr & 0xFFFF);
        }
    }
    return true;
}

bool CMipsMemoryVM::LD_PhysicalAddress(uint32_t PAddr, uint64_t & Value)
{
    if (PAddr < RdramSize())
    {
        *((uint32_t *)(&Value) + 1) = *(uint32_t *)(m_RDRAM + PAddr);
        *((uint32_t *)(&Value) + 0) = *(uint32_t *)(m_RDRAM + PAddr + 4);
        return true;
    }
    else if (PAddr < 0x800000)
    {
        Value = 0;
        return true;
    }

    g_Notify->BreakPoint(__FILE__, __LINE__);
    Value = 0;
    return false;
}

bool CMipsMemoryVM::SB_VAddr32(uint32_t VAddr, uint32_t Value)
{
    uint32_t BaseAddress = m_TLB_WriteMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_WMISS);
        return false;
    }
    return SB_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::SH_VAddr32(uint32_t VAddr, uint32_t Value)
{
    uint32_t BaseAddress = m_TLB_WriteMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_WMISS);
        return false;
    }
    return SH_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::SW_VAddr32(uint32_t VAddr, uint32_t Value)
{
    uint32_t BaseAddress = m_TLB_WriteMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_WMISS);
        return false;
    }
    return SW_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::SD_VAddr32(uint32_t VAddr, uint64_t Value)
{
    uint32_t BaseAddress = m_TLB_WriteMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        m_Reg.TriggerAddressException((int32_t)VAddr, EXC_WMISS);
        return false;
    }
    return SD_PhysicalAddress(BaseAddress + VAddr, Value);
}

bool CMipsMemoryVM::SB_PhysicalAddress(uint32_t PAddr, uint32_t Value)
{
    switch (PAddr & 0xFFF00000)
    {
    case 0x00000000:
    case 0x00100000:
    case 0x00200000:
    case 0x00300000:
    case 0x00400000:
    case 0x00500000:
    case 0x00600000:
    case 0x00700000:
        if (PAddr < RdramSize())
        {
            g_Recompiler->ClearRecompCode_Phys(PAddr & ~0xFFF, 0xFFC, CRecompiler::Remove_ProtectedMem);
            *(uint8_t *)(m_RDRAM + (PAddr ^ 3)) = (uint8_t)Value;
        }
        break;
    case 0x04000000: m_SPRegistersHandler.Write32(PAddr & ~3, Value << ((3 - (PAddr & 3)) * 8), 0xFFFFFFFF); break;
    case 0x1FC00000: m_PifRamHandler.Write32(PAddr & ~3, Value << ((3 - (PAddr & 3)) * 8), 0xFFFFFFFF); break;
    default:
        if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            m_RomMemoryHandler.Write32(PAddr, Value << ((3 - (PAddr & 3)) * 8), 0xFFFFFFFF);
        }
        else if (BreakOnUnhandledMemory())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
    }
    return true;
}

bool CMipsMemoryVM::SH_PhysicalAddress(uint32_t PAddr, uint32_t Value)
{
    switch (PAddr & 0xFFF00000)
    {
    case 0x00000000:
    case 0x00100000:
    case 0x00200000:
    case 0x00300000:
    case 0x00400000:
    case 0x00500000:
    case 0x00600000:
    case 0x00700000:
        if (PAddr < RdramSize())
        {
            if (CGameSettings::bSMM_StoreInstruc())
            {
                g_Recompiler->ClearRecompCode_Phys(PAddr & ~0xFFF, 0x1000, CRecompiler::Remove_ProtectedMem);
                m_TLB_WriteMap[(0x80000000 + PAddr) >> 12] = PAddr - (0x80000000 + PAddr);
                m_TLB_WriteMap[(0xA0000000 + PAddr) >> 12] = PAddr - (0xA0000000 + PAddr);
                *(uint16_t *)(m_RDRAM + (PAddr ^ 2)) = (uint16_t)Value;
            }
        }
        break;
    case 0x04000000: m_SPRegistersHandler.Write32(PAddr & ~3, Value << ((2 - (PAddr & 2)) * 8), 0xFFFFFFFF); break;
    case 0x1FC00000: m_PifRamHandler.Write32(PAddr & ~3, Value << ((2 - (PAddr & 2)) * 8), 0xFFFFFFFF); break;
    default:
        if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            m_RomMemoryHandler.Write32(PAddr, Value << ((2 - (PAddr & 2)) * 8), 0xFFFFFFFF);
        }
        else if (BreakOnUnhandledMemory())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
        return false;
    }

    return true;
}

bool CMipsMemoryVM::SW_PhysicalAddress(uint32_t PAddr, uint32_t Value)
{
    switch (PAddr & 0xFFF00000)
    {
    case 0x00000000:
    case 0x00100000:
    case 0x00200000:
    case 0x00300000:
    case 0x00400000:
    case 0x00500000:
    case 0x00600000:
    case 0x00700000:
    case 0x00800000:
        if (PAddr < RdramSize())
        {
            if (CGameSettings::bSMM_StoreInstruc())
            {
                g_Recompiler->ClearRecompCode_Phys(PAddr & ~0xFFF, 0x1000, CRecompiler::Remove_ProtectedMem);
                m_TLB_WriteMap[(0x80000000 + PAddr) >> 12] = PAddr - (0x80000000 + PAddr);
                m_TLB_WriteMap[(0xA0000000 + PAddr) >> 12] = PAddr - (0xA0000000 + PAddr);
                *(uint32_t *)(m_RDRAM + PAddr) = Value;
            }
        }
        break;
    case 0x03F00000: m_RDRAMRegistersHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04000000: m_SPRegistersHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04100000: m_DPCommandRegistersHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04300000: m_MIPSInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04400000: m_VideoInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04500000: m_AudioInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04600000: m_PeripheralInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04700000: m_RDRAMInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x04800000: m_SerialInterfaceHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x05000000: m_CartridgeDomain2Address1Handler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x06000000: m_CartridgeDomain1Address1Handler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x08000000:
    case 0x0fe00000:
        m_CartridgeDomain2Address2Handler.Write32(PAddr, Value, 0xFFFFFFFF);
        break;
    case 0x13F00000: m_ISViewerHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x1FC00000: m_PifRamHandler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    case 0x1FF00000: m_CartridgeDomain1Address3Handler.Write32(PAddr, Value, 0xFFFFFFFF); break;
    default:
        if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            m_RomMemoryHandler.Write32(PAddr, Value, 0xFFFFFFFF);
        }
        else if (BreakOnUnhandledMemory())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
        break;
    }
    return true;
}

bool CMipsMemoryVM::SD_PhysicalAddress(uint32_t PAddr, uint64_t Value)
{
    switch (PAddr & 0xFFF00000)
    {
    case 0x00000000:
    case 0x00100000:
    case 0x00200000:
    case 0x00300000:
    case 0x00400000:
    case 0x00500000:
    case 0x00600000:
    case 0x00700000:
        if (PAddr < RdramSize())
        {
            g_Recompiler->ClearRecompCode_Phys(PAddr & ~0xFFF, 0xFFC, CRecompiler::Remove_ProtectedMem);
            *(uint64_t *)(m_RDRAM + PAddr) = Value;
        }
        break;
    case 0x04000000: m_SPRegistersHandler.Write32(PAddr, (int32_t)(Value >> 32), 0xFFFFFFFF); break;
    default:
        if (PAddr >= 0x10000000 && PAddr < 0x20000000)
        {
            m_RomMemoryHandler.Write32(PAddr, (int32_t)(Value >> 32), 0xFFFFFFFF);
        }
        else if (BreakOnUnhandledMemory())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
        return false;
    }
    return true;
}

void CMipsMemoryVM::ClearMemoryWriteMap(uint32_t VAddr, uint32_t Length)
{
    uint32_t BaseAddress = m_TLB_WriteMap[VAddr >> 12];
    if (BaseAddress == -1)
    {
        return;
    }
    uint32_t PAddr = m_TLB_WriteMap[VAddr >> 12] + VAddr;
    for (uint32_t i = PAddr, n = (PAddr + (Length & ~0xFFF)) + 0x1000; i < n; i += 0x1000)
    {
        m_MemoryWriteMap[(i + 0x80000000) >> 12] = (size_t)-1;
        m_MemoryWriteMap[(i + 0xA0000000) >> 12] = (size_t)-1;
    }
}

const char * CMipsMemoryVM::LabelName(uint32_t Address) const
{
    sprintf(m_strLabelName, "0x%08X", Address);
    return m_strLabelName;
}

void CMipsMemoryVM::TLB_Mapped(uint32_t VAddr, uint32_t Len, uint32_t PAddr, bool bReadOnly)
{
    uint64_t VEnd = VAddr + Len;
    for (uint64_t Address = VAddr; Address < VEnd; Address += 0x1000)
    {
        size_t Index = (size_t)(Address >> 12);
        if ((Address - VAddr + PAddr) < m_AllocatedRdramSize)
        {
            m_MemoryReadMap[Index] = (size_t)((m_RDRAM + (Address - VAddr + PAddr)) - Address);
        }
        m_TLB_ReadMap[Index] = (size_t)((Address - VAddr + PAddr) - Address);
        if (!bReadOnly)
        {
            if ((Address - VAddr + PAddr) < m_AllocatedRdramSize)
            {
                m_MemoryWriteMap[Index] = (size_t)((m_RDRAM + (Address - VAddr + PAddr)) - Address);
            }
            m_TLB_WriteMap[Index] = (size_t)((Address - VAddr + PAddr) - Address);
        }
    }
}

void CMipsMemoryVM::TLB_Unmaped(uint32_t Vaddr, uint32_t Len)
{
    uint64_t End = Vaddr + Len;
    for (uint64_t Address = Vaddr; Address < End && Address >= Vaddr; Address += 0x1000)
    {
        size_t Index = (size_t)(Address >> 12);
        if (Address >= 0x80000000 && Address < 0x80000000 + m_AllocatedRdramSize)
        {
            m_MemoryReadMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
            m_MemoryWriteMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
            m_TLB_ReadMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
            m_TLB_WriteMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
        }
        else if (Address >= 0xA0000000 && Address < 0xA0000000 + m_AllocatedRdramSize)
        {
            m_MemoryReadMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
            m_MemoryWriteMap[Address >> 12] = (size_t)((m_RDRAM + (Address & 0x1FFFFFFF)) - Address);
            m_TLB_ReadMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
            m_TLB_WriteMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
        }
        else if (Address >= 0x80000000 && Address < 0xC0000000)
        {
            m_MemoryReadMap[Index] = (size_t)-1;
            m_MemoryWriteMap[Index] = (size_t)-1;
            m_TLB_ReadMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
            m_TLB_WriteMap[Address >> 12] = (uint32_t)((Address & 0x1FFFFFFF) - Address);
        }
        else
        {
            m_MemoryReadMap[Index] = (size_t)-1;
            m_MemoryWriteMap[Index] = (size_t)-1;
            m_TLB_ReadMap[Index] = (uint32_t)-1;
            m_TLB_WriteMap[Index] = (uint32_t)-1;
        }
    }
}

void CMipsMemoryVM::RdramChanged(CMipsMemoryVM * _this)
{
    const size_t new_size = g_Settings->LoadDword(Game_RDRamSize);
    const size_t old_size = _this->m_AllocatedRdramSize;

    if (old_size == new_size)
    {
        return;
    }
    if (old_size > new_size)
    {
        DecommitMemory(_this->m_RDRAM + new_size, old_size - new_size);
    }
    else
    {
        void * result = CommitMemory(_this->m_RDRAM + old_size, new_size - old_size, MEM_READWRITE);
        if (result == nullptr)
        {
            WriteTrace(TraceN64System, TraceError, "Failed to allocate extended memory");
            g_Notify->FatalError(GS(MSG_MEM_ALLOC_ERROR));
        }
    }

    if (new_size > 0xFFFFFFFFul)
    { // Should be unreachable because:  size_t new_size = g_Settings->(uint32_t)
        g_Notify->BreakPoint(__FILE__, __LINE__);
    } // However, FFFFFFFF also is a limit to RCP addressing, so we care
    _this->m_AllocatedRdramSize = (uint32_t)new_size;
}

void CMipsMemoryVM::ChangeMiIntrMask()
{
    if ((RegModValue & MI_INTR_MASK_CLR_SP) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_SP;
    }
    if ((RegModValue & MI_INTR_MASK_SET_SP) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_SP;
    }
    if ((RegModValue & MI_INTR_MASK_CLR_SI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_SI;
    }
    if ((RegModValue & MI_INTR_MASK_SET_SI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_SI;
    }
    if ((RegModValue & MI_INTR_MASK_CLR_AI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_AI;
    }
    if ((RegModValue & MI_INTR_MASK_SET_AI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_AI;
    }
    if ((RegModValue & MI_INTR_MASK_CLR_VI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_VI;
    }
    if ((RegModValue & MI_INTR_MASK_SET_VI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_VI;
    }
    if ((RegModValue & MI_INTR_MASK_CLR_PI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_PI;
    }
    if ((RegModValue & MI_INTR_MASK_SET_PI) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_PI;
    }
    if ((RegModValue & MI_INTR_MASK_CLR_DP) != 0)
    {
        g_Reg->MI_INTR_MASK_REG &= ~MI_INTR_MASK_DP;
    }
    if ((RegModValue & MI_INTR_MASK_SET_DP) != 0)
    {
        g_Reg->MI_INTR_MASK_REG |= MI_INTR_MASK_DP;
    }
}