#include "runtime_workspace.h"

// Ordinary RAM, not saved. Exploration's view and a battle's search take turns in it. It has its own
// translation unit, so host tests that define their own game state link without game.cpp.
RuntimeWorkspace runtime_workspace{};
