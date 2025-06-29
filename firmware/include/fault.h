#pragma once

#include <stdint.h>
#include <setjmp.h>

bool SafeMemoryRead(uint32_t address, uint8_t* buffer, uint32_t length);