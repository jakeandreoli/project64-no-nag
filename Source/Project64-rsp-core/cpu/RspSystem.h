#pragma once
#include <Project64-rsp-core/Hle/HleTask.h>
#include <Project64-rsp-core/RSPInfo.h>
#include <Project64-rsp-core/Recompiler/RspRecompilerCPU.h>
#include <Project64-rsp-core/cpu/RSPInterpreterOps.h>
#include <Project64-rsp-core/cpu/RSPRegisters.h>
#include <Project64-rsp-core/cpu/RspPipelineStage.h>
#include <Project64-rsp-core/cpu/RspTypes.h>
#include <stdint.h>

class RSPRegisterHandlerPlugin;

class CRSPSystem :
    public CHleTask
{
    friend class RSPOp;
    friend class CRSPRecompilerOps;
    friend class CRSPRecompiler;
    friend class CHleTask;
    friend class RSPDebuggerUI;
    friend class CRDPLog;
    friend class RSPRegisterHandler;
    friend class RSPRegisterHandlerPlugin;
    friend class CHle;

    friend void UpdateRSPRegistersScreen(void);

public:
    CRSPSystem();
    ~CRSPSystem();

    void Reset(RSP_INFO & Info);
    void RomClosed(void);

    void RunRecompiler(void);
    void ExecuteOps(uint32_t Cycles, uint32_t TargetPC);
    void SetupSyncCPU();
    bool IsSyncSystem(void);
    CRSPSystem * SyncSystem(void);
    void BasicSyncCheck(void);
    void * operator new(size_t size);
    void operator delete(void * ptr);

private:
    CRSPSystem(const CRSPSystem &);
    CRSPSystem & operator=(const CRSPSystem &);

    static void NullProcessDList(void);
    static void NullProcessRdpList(void);
    static void NullCheckInterrupts(void);

    CRSPSystem * m_SyncSystem;
    CRSPSystem * m_BaseSystem;
    CRSPRecompiler m_Recompiler;
    RSPRegisterHandlerPlugin * m_RSPRegisterHandler;
    CRSPRegisters m_Reg;
    RSPOp m_Op;
    RSPOpcode m_OpCode;
    RSPPIPELINE_STAGE m_NextInstruction;
    uint32_t m_JumpTo;
    uint8_t * m_HEADER;
    uint8_t * m_RDRAM;
    uint8_t * m_DMEM;
    uint8_t * m_IMEM;
    uint32_t * m_MI_INTR_REG;
    uint32_t * m_SP_MEM_ADDR_REG;
    uint32_t * m_SP_DRAM_ADDR_REG;
    uint32_t * m_SP_RD_LEN_REG;
    uint32_t * m_SP_WR_LEN_REG;
    uint32_t * m_SP_STATUS_REG;
    uint32_t * m_SP_DMA_FULL_REG;
    uint32_t * m_SP_DMA_BUSY_REG;
    uint32_t * m_SP_PC_REG;
    uint32_t * m_SP_SEMAPHORE_REG;
    uint32_t * m_DPC_START_REG;
    uint32_t * m_DPC_END_REG;
    uint32_t * m_DPC_CURRENT_REG;
    uint32_t * m_DPC_STATUS_REG;
    uint32_t * m_DPC_CLOCK_REG;
    uint32_t * m_DPC_BUFBUSY_REG;
    uint32_t * m_DPC_PIPEBUSY_REG;
    uint32_t * m_DPC_TMEM_REG;
    uint32_t m_RdramSize;
    uint32_t * m_SyncReg;
    void (*CheckInterrupts)(void);
    void (*ProcessDList)(void);
    void (*ProcessRdpList)(void);
};

extern CRSPSystem RSPSystem;