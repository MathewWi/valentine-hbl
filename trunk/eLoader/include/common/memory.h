#ifndef ELOADER_MEM
#define ELOADER_MEM

#include <common/sdk.h>

/* Overrides of sce functions to avoid syscall estimates */
SceSize sceKernelMaxFreeMemSize();
SceSize sceKernelTotalFreeMemSize();
int kill_thread(SceUID thid);
void UnloadModules();
#endif

