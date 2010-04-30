#include "eloader.h"
#include "elf.h"
#include "syscall.h"
#include "thread.h"
#include "debug.h"
#include "modmgr.h"

// Loaded modules descriptor
HBLModTable mod_table;

/* Find a module by name */
SceUID find_module(const char *name) 
{
	SceUID readbuf[256];
	int idcount;
	SceKernelModuleInfo info;
		
	sceKernelGetModuleIdList(readbuf, sizeof(readbuf)/sizeof(SceUID), &idcount);
	
	for(info.size=sizeof(info);idcount>0;idcount--)
	{
		
		if(sceKernelQueryModuleInfo(readbuf[idcount-1], &info) < 0)
			return -1;
		
        LOGSTR1("%s\n", info.name);
		
		if(strcmp(info.name, name) == 0)
			return readbuf[idcount-1];
	}
	
	return -1;
}

/*
// Get complete module info from an UID
int get_module_info(SceUID modid, tModuleInfo *modinfo)
{
	SceKernelModuleInfo info;
	void *ptr;
	
	// Get module info
	if(sceKernelQueryModuleInfo(modid, &info) < 0)
		return -1;
	
	// Create complete module info part
	modinfo->attribute = info.attribute;
	*((u16 *) &modinfo->version) = *((u16 *) &info.version);
	memcpy(&modinfo->name, &info.name, sizeof(modinfo->name));
	modinfo->gp_value = info.gp_value;
	
	// Get complete module info
	while(memcmp(ptr, modinfo, sizeof(tModuleInfo) - sizeof(u32)*4))
		ptr++;
	
	// Copy complete module info
	memcpy(modinfo, ptr, sizeof(tModuleInfo));
	
	return 0;
}
*/

/* Dump code by Fanjita and n00bz */
void DumpModuleList()
{
  int    i;
  unsigned long *lstartptr;
  unsigned long *lendptr;
  unsigned long *lptr;
  char *lcharptr;
  unsigned long lversionAndAttr;
  SceUID gmoduleids[256];
  int gmodulecount = 0;
  SceKernelModuleInfo info;

  LOGSTR0("DUMP MODULE OBJECTS:\n");
  LOGSTR0("=======================\n");
  
  sceKernelGetModuleIdList(gmoduleids, sizeof(gmoduleids)/sizeof(SceUID), &gmodulecount);
  LOGSTR1("Got %d modules\n", gmodulecount);
  for (i=0; i < gmodulecount; i++)
  {
    memset(&info, 0, sizeof(SceKernelModuleInfo));
    info.size = sizeof(SceKernelModuleInfo);
    LOGSTR1("---\nRetrieve module ID %d\n", i);
    sceKernelQueryModuleInfo(gmoduleids[i], &info);

    LOGSTR2("%d entry: %p  ", i, info.entry_addr);
    LOGSTR2("mod id: %08lX  name: %s\n", gmoduleids[i],info.name);

    LOGSTR2("segm start: %08lX   end: %08lX\n", info.text_addr, (info.text_addr + info.text_size));
  }
}

// Return index in mod_table for module ID
int get_module_index(SceUID modid)
{
	int i;

	for (i=0; i<mod_table.num_loaded_mod; i++)
	{
		if (mod_table.table[i].id == modid)
			return i;
	}

	return -1;
}

// Initialize module loading data structures
void init_load_module()
{
	memset(&mod_table, 0, sizeof(mod_table));
}

// Loads a module to memory
SceUID load_module(SceUID elf_file, const char* path, void* addr, SceOff offset)
{
	LOGSTR0("\n\n->Entering load_module...\n");
	
	if (mod_table.num_loaded_mod >= MAX_MODULES)
		return SCE_KERNEL_ERROR_EXCLUSIVE_LOAD;
	
	// Read ELF header
	Elf32_Ehdr elf_hdr;
	sceIoRead(elf_file, &elf_hdr, sizeof(elf_hdr));

	LOGELFHEADER(elf_hdr);
	
	// Loading module
	tStubEntry* pstub;
	unsigned int hbsize, program_size, stubs_size;
	unsigned int i = mod_table.num_loaded_mod;
	
	// Static ELF
	if(elf_hdr.e_type == (Elf32_Half) ELF_STATIC)
	{
		LOGSTR0("STATIC\n");
		
		if(mod_table.num_loaded_mod > 0)
			return SCE_KERNEL_ERROR_UNKNOWN_MODULE;

		// Load ELF program section into memory
		hbsize = elf_load_program(elf_file, offset, &elf_hdr);		
	
		// Locate ELF's .lib.stubs section
		stubs_size = elf_find_imports(elf_file, offset, &elf_hdr, &pstub);
		
		mod_table.table[i].text_entry = (u32)elf_hdr.e_entry;
		mod_table.table[i].gp = (void*)getGP(elf_file, offset, &elf_hdr);
	}

	// Relocatable ELF (PRX)
	else if(elf_hdr.e_type == (Elf32_Half) ELF_RELOC)
	{
		LOGSTR0("RELOC\n");

		LOGSTR1("load_module -> Offset: 0x%08lX\n", offset);
		
		// Load PRX program section
		if ((stubs_size = prx_load_program(elf_file, offset, &elf_hdr, &pstub, &program_size, &addr)) == 0)
			return SCE_KERNEL_ERROR_ERROR;

		sceKernelDcacheWritebackInvalidateAll();

		LOGSTR1("Before reloc -> Offset: 0x%08lX\n", offset);
		//Relocate all sections that need to
		unsigned int ret = relocate_sections(elf_file, offset, &elf_hdr);
		if (ret == 0)
		{
			LOGSTR0("WARNING: no sections to relocate in a relocatable ELF o_O\n");
		}

		sceKernelDcacheWritebackInvalidateAll();
		
		// Relocate ELF entry point and GP register
		mod_table.table[i].text_entry = (u32)elf_hdr.e_entry + (u32)addr;
        mod_table.table[i].gp = getGP(elf_file, offset, &elf_hdr) + (u32)addr;		
	}

	// Unknown ELF type
	else
	{
		exit_with_log(" UNKNOWN ELF TYPE ", NULL, 0);
	}
	
	// Resolve ELF's stubs with game's stubs and syscall estimation
	unsigned int stubs_resolved = resolve_imports(pstub, stubs_size);

	if (stubs_resolved == 0)
		LOGSTR0("WARNING: no stubs found!!\n");

	//LOGSTR0("\nUpdating module table\n");

	mod_table.table[i].id = MOD_ID_START + i;
	mod_table.table[i].state = LOADED;
	mod_table.table[i].size = program_size;
	mod_table.table[i].text_addr = addr;
	mod_table.table[i].libstub_addr = pstub;	
	strcpy(mod_table.table[i].path, path);	
	mod_table.num_loaded_mod++;

	//LOGSTR0("Module table updated\n");

	LOGSTR1("->Actual number of loaded modules: %d\n", mod_table.num_loaded_mod);
	LOGSTR1("Last loaded module [%d]:\n", i);
	LOGMODENTRY(mod_table.table[i]);
  
	// No need for ELF file anymore
	sceIoClose(elf_file);

	sceKernelDcacheWritebackInvalidateAll();
	
	return mod_table.table[i].id;
}

// Starts an already loaded module
SceUID start_module(SceUID modid)
{
	LOGSTR1("\n\n-->Starting module ID: 0x%08lX\n", modid);
	
	if (mod_table.num_loaded_mod == 0)
		return SCE_KERNEL_ERROR_UNKNOWN_MODULE;

	int index = get_module_index(modid);

	if (index < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_MODULE;

	if (mod_table.table[index].state == RUNNING)
		return SCE_KERNEL_ERROR_ALREADY_STARTED;

	LOGMODENTRY(mod_table.table[index]);

	u32 gp_bak;
	
	GET_GP(gp_bak);
	SET_GP(mod_table.table[index].gp);

	SceUID thid = sceKernelCreateThread("hblmodule", mod_table.table[index].text_entry, 0x30, 0x1000, 0, NULL);

	if(thid >= 0)
	{
		LOGSTR1("->MODULE MAIN THID: 0x%08lX ", thid);
		thid = sceKernelStartThread(thid, strlen(mod_table.table[index].path) + 1, (void *)mod_table.table[index].path);
		if (thid < 0)
		{
			LOGSTR1(" HB Thread couldn't start. Error 0x%08lX\n", thid);
			return thid;
		}
    }
	else 
	{
        LOGSTR1(" HB Thread couldn't be created. Error 0x%08lX\n", thid);
		return thid;
	}

	SET_GP(gp_bak);

	return modid;
}
