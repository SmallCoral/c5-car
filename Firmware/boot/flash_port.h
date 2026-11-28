/* 引导程序的 Flash 读取、擦页和四字编程接口，操作结果供镜像安装器判断是否继续。 */

#ifndef CORAL_FLASH_PORT_H
#define CORAL_FLASH_PORT_H

#include "ota_store.h"

/* 从许可的 Flash 区间读取，并将本次读取触发的 ECC 错误传回上层。 */
bool flash_port_read(uint32_t address, void *data, size_t length);
/* 校验地址、页边界和许可区域后擦除对应页。 */
bool flash_port_erase(uint32_t page);
/* 校验目标地址及四字对齐要求，送入 RAM 中的 Flash 写操作。 */
bool flash_port_program(uint32_t address, const void *data, size_t length);

#endif
