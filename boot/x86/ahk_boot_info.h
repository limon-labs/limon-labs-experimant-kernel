#ifndef AHK_BOOT_INFO_H
#define AHK_BOOT_INFO_H

typedef struct {
    uint32_t magic;            // 0xAHK1 (0x41484B31)
    uint32_t kernel_size;      // kernel size in bytes
    uint32_t entry_point;      // kernel entry address (0x100000)
    uint32_t stack_top;        // kernel stack address
    uint32_t memory_map_addr;  // physical memory map address
    uint32_t framebuffer_addr; // VGA framebuffer (0xB8000)
    uint32_t cis_config;       // CIS configuration flags
} PACKED ahk_boot_info_t;

#endif /* AHK_BOOT_INFO_H */
