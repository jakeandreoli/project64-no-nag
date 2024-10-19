#include "RSPCpu.h"
#include <Common/CriticalSection.h>
#include <Project64-rsp-core/RSPDebugger.h>
#include <Project64-rsp-core/RSPInfo.h>
#include <Project64-rsp-core/Recompiler/RspRecompilerCPU.h>
#include <Project64-rsp-core/Settings/RspSettings.h>
#include <Project64-rsp-core/cpu/RSPRegisters.h>
#include <Project64-rsp-core/cpu/RspSystem.h>
#include <memory>

class RSPRegisterHandler;

UDWORD EleSpec[16], Indx[16];
uint32_t RSP_Running;

CriticalSection g_CPUCriticalSection;
uint32_t Mfc0Count, SemaphoreExit = 0;

void Build_RSP(void)
{
    extern UWORD32 Recp, RecpResult, SQroot, SQrootResult;

    Recp.UW = 0;
    RecpResult.UW = 0;
    SQroot.UW = 0;
    SQrootResult.UW = 0;

    if (g_RSPDebugger != nullptr)
    {
        g_RSPDebugger->ResetTimerList();
    }

    EleSpec[0].DW = 0x0001020304050607;  // None
    EleSpec[1].DW = 0x0001020304050607;  // None
    EleSpec[2].DW = 0x0000020204040606;  // 0q
    EleSpec[3].DW = 0x0101030305050707;  // 1q
    EleSpec[4].DW = 0x0000000004040404;  // 0h
    EleSpec[5].DW = 0x0101010105050505;  // 1h
    EleSpec[6].DW = 0x0202020206060606;  // 2h
    EleSpec[7].DW = 0x0303030307070707;  // 3h
    EleSpec[8].DW = 0x0000000000000000;  // 0
    EleSpec[9].DW = 0x0101010101010101;  // 1
    EleSpec[10].DW = 0x0202020202020202; // 2
    EleSpec[11].DW = 0x0303030303030303; // 3
    EleSpec[12].DW = 0x0404040404040404; // 4
    EleSpec[13].DW = 0x0505050505050505; // 5
    EleSpec[14].DW = 0x0606060606060606; // 6
    EleSpec[15].DW = 0x0707070707070707; // 7

    Indx[0].DW = 0x0001020304050607;  // None
    Indx[1].DW = 0x0001020304050607;  // None
    Indx[2].DW = 0x0103050700020406;  // 0q
    Indx[3].DW = 0x0002040601030507;  // 1q
    Indx[4].DW = 0x0102030506070004;  // 0h
    Indx[5].DW = 0x0002030406070105;  // 1h
    Indx[6].DW = 0x0001030405070206;  // 2h
    Indx[7].DW = 0x0001020405060307;  // 3h
    Indx[8].DW = 0x0102030405060700;  // 0
    Indx[9].DW = 0x0002030405060701;  // 1
    Indx[10].DW = 0x0001030405060702; // 2
    Indx[11].DW = 0x0001020405060703; // 3
    Indx[12].DW = 0x0001020305060704; // 4
    Indx[13].DW = 0x0001020304060705; // 5
    Indx[14].DW = 0x0001020304050706; // 6
    Indx[15].DW = 0x0001020304050607; // 7

    for (uint8_t i = 0, n = sizeof(EleSpec) / sizeof(EleSpec[0]); i < n; i++)
    {
        for (uint8_t z = 0; z < 8; z++)
        {
            Indx[i].B[z] = 7 - Indx[i].B[z];
            EleSpec[i].B[z] = 7 - EleSpec[i].B[z];
        }
        for (uint8_t z = 0; z < 4; z++)
        {
            uint8_t Temp = Indx[i].B[z];
            Indx[i].B[z] = Indx[i].B[7 - z];
            Indx[i].B[7 - z] = Temp;
        }
    }
}

/*
Function: DoRspCycles
Purpose: This function is to allow the RSP to run in parallel with
the r4300i switching control back to the r4300i once the
function ends.
Input: The number of cycles that is meant to be executed.
Output: The number of cycles that was executed. This value can
be greater than the number of cycles that the RSP should have performed.
(this value is ignored if the RSP has been stopped)
*/

#define MI_INTR_SP 0x01 /* Bit 0: SP intr */

uint32_t DoRspCycles(uint32_t Cycles)
{
    if (RSPSystem.IsHleTask() && RSPSystem.ProcessHleTask())
    {
        return Cycles;
    }

    if (g_RSPDebugger != nullptr)
    {
        g_RSPDebugger->RspCyclesStart();
    }
    CGuard Guard(g_CPUCriticalSection);

    switch (CRSPSettings::CPUMethod())
    {
    case RSPCpuMethod::Recompiler:
        RSPSystem.RunRecompiler();
        break;
    case RSPCpuMethod::Interpreter:
    case RSPCpuMethod::RecompilerTasks:
    case RSPCpuMethod::HighLevelEmulation:
        RSPSystem.ExecuteOps((uint32_t)-1, (uint32_t)-1);
        break;
    }
    if (g_RSPDebugger != nullptr)
    {
        g_RSPDebugger->RspCyclesStop();
    }
    return Cycles;
}
