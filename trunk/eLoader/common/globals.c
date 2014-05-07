#include <common/globals.h>
#include <common/debug.h>
#include <common/utils.h>
#include <hbl/stubs/syscall.h>
#include <exploit_config.h>

tGlobals * get_globals()
{
    return (tGlobals *) GLOBALS_ADDR;
}

void init_globals()
{
    if ((u32)GLOBALS_ADDR < 0x14000)
    {
        if (HBL_STUBS_START + NUM_HBL_IMPORTS*2*sizeof(u32) > (u32)GLOBALS_ADDR)
        {
            exit_with_log("FATAL: Increase GLOBALS_ADDR", NULL, 0);
        }

        u32 size = sizeof(tGlobals);
        if (size + (u32)GLOBALS_ADDR >= 0x14000)
        {
            logstr1("Globals structure doesn't fit in Scratchpad RAM: 0x%08lX\n", size);
            exit_with_log("FATAL: Globals too big", NULL, 0);
        }
    }
    tGlobals * g = get_globals();
    memset(g, 0, sizeof(tGlobals));
#ifdef VITA
	strcpy(g->menupath, HBL_ROOT"EBOOT.PBP");
#else
    strcpy(g->menupath, HBL_ROOT"menu/EBOOT.PBP");
#endif
#ifndef VITA
	// Intialize firmware and model
	getFirmwareVersion();
	getPSPModel();

#ifndef DEACTIVATE_SYSCALL_ESTIMATION
	// Select syscall estimation method
	g->syscalls_known = 0;

	int i;
	unsigned short syscalls_known_for_firmwares[] = SYSCALLS_KNOWN_FOR_FIRMWARES;
	unsigned short syscalls_known_for_go_firmwares[] = SYSCALLS_KNOWN_FOR_GO_FIRMWARES;

	unsigned short* firmware_array;
	int firmware_array_size;
	if (getPSPModel() == PSP_GO)
	{
		firmware_array = syscalls_known_for_go_firmwares;
		firmware_array_size = sizeof(syscalls_known_for_go_firmwares) / sizeof(unsigned short);
	}
	else
	{
		firmware_array = syscalls_known_for_firmwares;
		firmware_array_size = sizeof(syscalls_known_for_firmwares) / sizeof(unsigned short);
	}

	for (i = 0; i < firmware_array_size; i++)
	{
		if (firmware_array[i] == getFirmwareVersion())
		{
			g->syscalls_known = 1;
			break;
		}
	}
#endif
#endif
	g->syscalls_from_p5 = 1; // Always available

	g->exit_callback_called = 0;

    g->curr_channel_id = -1;

#ifdef VITA
    reset_vita_dirs();
#endif
}

#ifdef VITA
void reset_vita_dirs()
{
    tGlobals * g = get_globals();
    int i,j;

    for (i=0;i<MAX_OPEN_DIR_VITA;++i)
        for (j=0;j<2;++j)
            g->directoryFix[i][j] = -1;
}
#endif