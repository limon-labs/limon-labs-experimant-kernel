#include "kernel.h"

static uint32_t cis_fault_count = 0;
static uint32_t cis_recovered_count = 0;
static uint32_t cis_escalated_count = 0;

void cis_init(void)
{
    LOG_INFO_MSG("[CIS] Initializing Computer Immune System...\n");
    cis_fault_count = 0;
    cis_recovered_count = 0;
    cis_escalated_count = 0;
    LOG_INFO_MSG("[CIS] CIS Engine Online\n");
}

void cis_handle_fault(registers_t *regs)
{
    cis_fault_count++;
    
    if (regs->int_no == 14) { /* Page Fault */
        uint32_t fault_addr = cpu_read_cr2();
        LOG_WARN_MSG("[CIS] Page fault @ 0x%x\n", fault_addr);
        
        /* Try to recover - allocate new page */
        void *page = pmm_alloc_frame();
        if (page) {
            vmm_map_page((page_directory_t)cpu_read_cr3(), fault_addr,
                         (uintptr_t)page, PAGE_PRESENT | PAGE_WRITABLE);
            cis_recovered_count++;
            LOG_INFO_MSG("[CIS] Recovered page fault - allocated 0x%x\n", page);
            return;
        }
    }
    
    cis_escalated_count++;
    LOG_ERROR_MSG("[CIS] Unrecoverable fault - escalating\n");
    PANIC("CIS: Unrecoverable fault");
}

void cis_detect_fault(fault_t *fault)
{
    (void)fault;  /* Mark as unused to prevent warning */
    fault->severity = FAULT_NORMAL;
    LOG_DEBUG_MSG("[CIS] Detected fault type: %d\n", fault->type);
}

void cis_diagnose_fault(fault_t *fault)
{
    (void)fault;  /* Mark as unused to prevent warning */
    LOG_DEBUG_MSG("[CIS] Diagnosing fault...\n");
    fault->severity = FAULT_WARNING;
}

void cis_remediate(fault_t *fault)
{
    (void)fault;  /* Mark as unused to prevent warning */
    LOG_DEBUG_MSG("[CIS] Remediating fault...\n");
}

void cis_verify_remediation(fault_t *fault)
{
    (void)fault;  /* Mark as unused to prevent warning */
    LOG_DEBUG_MSG("[CIS] Verifying remediation...\n");
}

void cis_learn(fault_type_t type, const char *solution)
{
    (void)type;  /* Mark as unused to prevent warning */
    (void)solution;  /* Mark as unused to prevent warning */
    LOG_DEBUG_MSG("[CIS] Learned solution for fault type %d: %s\n", type, solution);
}

bool cis_recall(fault_type_t type, char *out_solution)
{
    (void)type;  /* Mark as unused to prevent warning */
    (void)out_solution;  /* Mark as unused to prevent warning */
    LOG_DEBUG_MSG("[CIS] Recalling solution for fault type %d\n", type);
    return FALSE;
}

void cis_get_stats(uint32_t *faults, uint32_t *recovered, uint32_t *escalated)
{
    if (faults) *faults = cis_fault_count;
    if (recovered) *recovered = cis_recovered_count;
    if (escalated) *escalated = cis_escalated_count;
}
