// Saved identities and flag indices follow this order. Append entries only.
ENEMY_SPAWN(positions::GroveWolf, &BATTLE_GROVE_WOLVES, 17)
ENEMY_SPAWN(positions::MineScout, &BATTLE_MINE_SCOUTS, 17, ENEMY_CHASE, SPEED_FAST)
ENEMY_SPAWN(positions::VaultGuard, &BATTLE_VAULT_GUARDS, 25, ENEMY_ROUTE, SPEED_NORMAL,
            routes::VaultPatrol, RESPAWN_NEVER, VaultGuardHooks)
