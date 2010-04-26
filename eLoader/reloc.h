#ifndef ELOADER_RELOC
#define ELOADER_RELOC

// Relocates PRX sections that need to
// Returns number of relocated entries
unsigned int relocate_sections(SceUID elf_file, u32 start_offset, Elf32_Ehdr *pelf_header);

#endif