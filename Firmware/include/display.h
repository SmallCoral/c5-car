#ifndef CORAL_DISPLAY_H
#define CORAL_DISPLAY_H

#include "ui.h"

bool display_init(void);
bool display_poll(const struct ui_state *state, uint32_t now);

#endif
