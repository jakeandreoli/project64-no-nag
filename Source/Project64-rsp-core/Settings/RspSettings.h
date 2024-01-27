#pragma once
#include <stdint.h>

void InitializeRspSetting(void);

extern uint16_t Set_AudioHle, Set_GraphicsHle, Set_AllocatedRdramSize, Set_DirectoryLog;
extern bool GraphicsHle, AudioHle, ConditionalMove, HleAlistTask, RspMultiThreaded;
extern bool DebuggingEnabled, Profiling, IndvidualBlock, ShowErrors, BreakOnStart, LogRDP, LogX86Code;