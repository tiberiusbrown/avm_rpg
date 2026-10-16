#include "rpg.h"

void dialogue_talk(Game& g, uint8_t id) {
    if(id != 1) {
        dialogue_show(g, F("I walk a little, then rest a while. The house is east."));
        return;
    }

    if(g.flags & FLAG_HELPED) {
        dialogue_show(g, F("The lantern is lit. Thanks for helping! I remember you."));
        return;
    }

    if(!dialogue_show(g, F("Welcome to our small test house. The path is outside. "
                           "We keep a lantern here for travelers returning after dark. "
                           "A little light helps everyone find their way home.")))
        return;

    if(dialogue_ask(g, F("Will you light the travelers' lantern? Your answer stays."))) {
        g.flags |= FLAG_HELPED;
        g.gold += 5;
        dialogue_show(g, F("The lantern is lit. Thanks for helping! I remember you."));
    } else {
        dialogue_show(g, F("Perhaps another day. The door stays open."));
    }
}
