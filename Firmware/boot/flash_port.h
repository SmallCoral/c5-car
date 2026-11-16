#ifndef CORAL_FLASH_PORT_H
#define CORAL_FLASH_PORT_H

#include "ota_store.h"

bool flash_port_read(uint32_t address, void *data, size_t length);
bool flash_port_erase(uint32_t page);
bool flash_port_program(uint32_t address, const void *data, size_t length);

#endif
