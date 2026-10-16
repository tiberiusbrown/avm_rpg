#ifndef NPC_LOGIC_H
#define NPC_LOGIC_H

#include "npc.h"

// Synchronous hooks: no blocking UI, reset/teleport, or recursive simulation.
// Activation and movement events may change this actor's behavior via npc_* APIs.
// Retirement observes the outgoing actor; restore runs after the pool is cleared.
void npc_on_activate(Npc& npc);
void npc_on_event(Npc& npc, NPC_EVENT event);
void npc_on_retire(Npc const& npc, NPC_RETIRE_REASON reason);
void npc_logic_restore();
void npc_talk(NPC_ID id);

#endif
