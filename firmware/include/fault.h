#pragma once

#include <setjmp.h>
#include <stdint.h>

bool SafeMemoryRead(uint32_t address, uint8_t* buffer, uint32_t length);