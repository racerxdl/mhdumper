
extern "C" {
#include "air105.h"
#include "irq.h"
}

#include <cstdio>

#include <setjmp.h>
// Global jump buffer for memory fault recovery
static jmp_buf error_recovery_buf;
static jmp_buf* error_jmp_buf_ptr = &error_recovery_buf;

// Memory fault handlers
void MemManage_Handler(void) {
    // Clear the fault status
    SCB->CFSR |= SCB_CFSR_MEMFAULTSR_Msk;

    // Jump back to safe point
    if (error_jmp_buf_ptr) {
        longjmp(*error_jmp_buf_ptr, 1);
    }
    while(1);
}

void BusFault_Handler(void) {
    SCB->CFSR |= SCB_CFSR_BUSFAULTSR_Msk;
    if (error_jmp_buf_ptr) {
        longjmp(*error_jmp_buf_ptr, 1);
    }
    while(1);
}

void UsageFault_Handler(void) {
    SCB->CFSR |= SCB_CFSR_USGFAULTSR_Msk;
    if (error_jmp_buf_ptr) {
        longjmp(*error_jmp_buf_ptr, 1);
    }
    while(1);
}


// Safe memory reading function
bool SafeMemoryRead(uint32_t address, uint8_t* buffer, uint32_t length) {
    volatile bool hasFaults = false;
    volatile uint32_t offset = 0;
    // Initialize buffer with zeros (default for inaccessible memory)
    memset(buffer, 0, length);

    // Read memory dword by dword
    while (offset < length) {
        volatile uint32_t current_addr = address + offset;
        volatile uint32_t bytes_to_read = ((offset + 4) <= length) ? 4 : (length - offset);

        // Set recovery point
        if (setjmp(error_recovery_buf) == 0) {
            if (bytes_to_read == 4 && (current_addr & 0x3) == 0) {
                // 4-byte aligned read - use direct 32-bit access for better performance
                uint32_t value = *(volatile uint32_t*)current_addr;
                memcpy(&buffer[offset], &value, 4);
            } else {
                // Unaligned or partial read - fall back to byte-by-byte access
                for (volatile uint32_t i = 0; i < bytes_to_read; i++) {
                    buffer[offset + i] = *((volatile uint8_t*)current_addr + i);
                }
            }
        } else {
            // A fault occurred
            hasFaults = true;
            // The memory at this location remains zeroed from the initial memset
        }

        // Move to next block
        offset += 4;
    }

    return hasFaults;
}
