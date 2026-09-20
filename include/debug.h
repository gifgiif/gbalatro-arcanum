#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>

#ifndef GBALATRO_DEBUG_MENU
#define GBALATRO_DEBUG_MENU 1
#endif

/*
 * Returns true while debug input owns the frame. The caller must then skip the
 * normal state-machine input so A/B presses cannot affect the game underneath.
 */
bool debug_update(void);
void debug_reset(void);

#endif // DEBUG_H
