// Deterministic review boards. Only setup uses the QA memory interface. The guest selects and
// resolves each action through its ordinary battle input, and Ardens supplies the display/audio.
struct ReviewUnit {
    uint8_t cell, hp;
    bool ally = false;
    ATTACK_CLASS attack = ATTACK_STRIKE;
    uint8_t attack_rank = 0;
};

constexpr uint8_t c(uint8_t x, uint8_t y) { return battle_cell(x, y); }
constexpr ReviewUnit foe(uint8_t x, uint8_t y, uint8_t hp = 10) { return {c(x, y), hp}; }
constexpr ReviewUnit ally(uint8_t x, uint8_t y, uint8_t hp = 14,
                         ATTACK_CLASS attack = ATTACK_STRIKE, uint8_t rank = 0) {
    return {c(x, y), hp, true, attack, rank};
}

struct ReviewBoard {
    uint8_t from, to;
    std::initializer_list<ReviewUnit> units;
    uint8_t hp = 18;
    BATTLEFIELD_ID field = BATTLEFIELD_GROVE;
};

struct MoveReview {
    char const* title;
    MOVEMENT_CLASS move;
    uint8_t rank;
    ReviewBoard board;
    bool enemy_reply = false;
};

static const MoveReview MOVEMENT_REVIEW[] = {
    {"Step | Quiet move and its HP cost", MOVEMENT_STEP, 0, {c(2,1), c(3,1), {}}},
    {"Leap | Knight jump into engagement", MOVEMENT_LEAP, 0, {c(1,1), c(3,2), {foe(4,2)}}},
    {"Charge | Run into engagement", MOVEMENT_CHARGE, 0, {c(1,1), c(3,1), {foe(4,1)}}},
    {"Vault | Jump over a fighter", MOVEMENT_VAULT, 0, {c(1,1), c(3,1), {foe(2,1)}}},
    {"Glide | Long diagonal", MOVEMENT_GLIDE, 0, {c(2,0), c(4,2), {foe(5,2)}}},
    {"Footwork | Engaging Step waives the HP cost", MOVEMENT_STEP, 2, {c(2,1), c(3,1), {foe(4,1)}}},
    {"Hold Together | Supported defender takes a direct hit", MOVEMENT_STEP, 1,
     {c(6,1), c(7,1), {ally(7,2), foe(7,0)}}, true},
    {"Shield Wall | Stronger reduction with the same setup", MOVEMENT_STEP, 3,
     {c(6,1), c(7,1), {ally(7,2), foe(7,0)}}, true},
    {"Shield Rebound | The supported fighter reflects damage", MOVEMENT_STEP, 4,
     {c(6,1), c(7,1), {ally(7,2), foe(7,0)}}, true},
    {"Landing Strike | One landing neighbor", MOVEMENT_LEAP, 1, {c(1,1), c(3,2), {foe(4,2)}}},
    {"Wide Landing | Two landing neighbors", MOVEMENT_LEAP, 2, {c(1,1), c(3,2), {foe(4,2), foe(3,3)}}},
    {"Skyfall | Three landing neighbors", MOVEMENT_LEAP, 3, {c(1,1), c(3,2), {foe(4,2), foe(3,3), foe(3,1)}}},
    {"Skyhook | Single survivor pulled to takeoff", MOVEMENT_LEAP, 4, {c(1,1), c(3,2), {foe(4,2)}}},
    {"Spearhead and Momentum | Forward and side hits", MOVEMENT_CHARGE, 2,
     {c(1,1), c(3,1), {foe(4,1), foe(3,2)}}},
    {"Overrun | Push the forward survivor", MOVEMENT_CHARGE, 3, {c(1,1), c(3,1), {foe(4,1)}}},
    {"Trample | Cross one enemy and push another", MOVEMENT_CHARGE, 4,
     {c(1,1), c(3,1), {foe(2,1), foe(4,1)}}},
    {"Vault Strike and Vault Force | Crossed and neighboring enemies", MOVEMENT_VAULT, 2,
     {c(1,1), c(3,1), {foe(2,1), foe(3,2)}}},
    {"Shock Landing | Diagonal engagement", MOVEMENT_VAULT, 3,
     {c(1,1), c(3,1), {foe(2,1), foe(4,2)}}},
    {"High Vault | Three-cell landing over two enemies", MOVEMENT_VAULT, 4,
     {c(1,1), c(4,1), {foe(2,1), foe(3,1), foe(5,1)}}},
    {"Cutback | Short Glide into one enemy", MOVEMENT_GLIDE, 1, {c(2,1), c(3,2), {foe(4,2)}}},
    {"Slipstream | Long Glide into two enemies", MOVEMENT_GLIDE, 2,
     {c(2,0), c(4,2), {foe(5,2), foe(4,1)}}},
    {"Passing Cut | Hits along the route and at landing", MOVEMENT_GLIDE, 3,
     {c(2,0), c(4,2), {foe(3,0), foe(5,2)}}},
    {"Diamond Wake | Catch the enemy diagonally behind takeoff", MOVEMENT_GLIDE, 4,
     {c(2,0), c(4,2), {foe(1,1), foe(3,0), foe(5,2)}}},
};

struct AttackReview {
    char const* title;
    ATTACK_CLASS attack;
    uint8_t rank;
    ReviewBoard board;
};

static const AttackReview NORMAL_REVIEW[] = {
    {"Strike | Sword hit on a healthy target", ATTACK_STRIKE, 0, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Shoot | Arrow at range four", ATTACK_SHOOT, 0, {c(1,1), c(5,1), {foe(5,1)}}},
    {"Reach | Thrust at range two", ATTACK_REACH, 0, {c(2,1), c(4,1), {foe(4,1)}}},
    {"Impact | Knock a survivor back", ATTACK_IMPACT, 0, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Rend | Bite an injured enemy", ATTACK_REND, 0, {c(2,1), c(3,1), {foe(3,1,4)}}},
    {"Arc | Sweep diagonally", ATTACK_ARC, 0, {c(2,1), c(3,2), {foe(3,2)}}},
    {"Drain | Heal from a surviving enemy", ATTACK_DRAIN, 0, {c(2,1), c(3,1), {foe(3,1)}, 12}},
    {"Recoil | Hit and retreat", ATTACK_RECOIL, 0, {c(3,1), c(4,1), {foe(4,1)}}},
    {"Flank | Strike with an ally beside the enemy", ATTACK_FLANK, 0,
     {c(3,1), c(4,1), {foe(4,1), ally(4,2)}}},
    {"Blast | Orb that splashes an adjacent enemy", ATTACK_BLAST, 0,
     {c(2,1), c(5,1), {foe(5,1), foe(5,2)}}},
};

static const AttackReview STARTER_REVIEW[] = {
    {"Power Strike | Heavy sword hit", ATTACK_STRIKE, 0, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Opening Shot | Full-health bonus at long range", ATTACK_SHOOT, 2, {c(1,1), c(5,1), {foe(5,1)}}},
    {"Driving Thrust | A survivor is knocked back", ATTACK_REACH, 1, {c(2,1), c(4,1), {foe(4,1)}}},
    {"Crushing Bash | Hard impact with room to push", ATTACK_IMPACT, 0, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Wound Hunter | Blood Hunt against a wounded target at range two", ATTACK_REND, 4,
     {c(2,1), c(4,1), {foe(4,1,4)}}},
    {"Crescent Cut | Corner Sweep catches both neighbors", ATTACK_ARC, 4,
     {c(2,1), c(3,2), {foe(3,2), foe(2,2), foe(3,1)}}},
    {"Siphon | Deep Siphon heals the attacker", ATTACK_DRAIN, 3, {c(2,1), c(3,1), {foe(3,1)}, 12}},
    {"Spring Blow | Springback retreats two cells", ATTACK_RECOIL, 3, {c(4,1), c(5,1), {foe(5,1)}}},
    {"Pincer Strike | Blind Side attacks through a supporting ally", ATTACK_FLANK, 4,
     {c(2,1), c(4,1), {foe(4,1), ally(3,1), ally(4,2)}}},
    {"Detonation | Concussion knocks splash victims outward", ATTACK_BLAST, 4,
     {c(2,1), c(5,1), {foe(5,1), foe(5,2), foe(6,2)}}},
    {"Opening Shot | Smaller bonus against an injured target", ATTACK_SHOOT, 0,
     {c(2,0), c(2,2), {foe(2,2,7)}}},
    {"Driving Thrust | The board edge stops the knockback", ATTACK_REACH, 0,
     {c(5,1), c(7,1), {foe(7,1)}}},
    {"Crushing Bash | Collision hits the enemy behind the target", ATTACK_IMPACT, 3,
     {c(2,1), c(3,1), {foe(3,1), foe(4,1)}}},
    {"Wound Hunter | Smaller bonus against a healthy target", ATTACK_REND, 0,
     {c(2,1), c(3,1), {foe(3,1)}}},
    {"Crescent Cut | A clean diagonal with empty corners", ATTACK_ARC, 2,
     {c(3,2), c(2,1), {foe(2,1)}}},
    {"Spring Blow | An ally blocks the retreat", ATTACK_RECOIL, 3,
     {c(4,1), c(5,1), {foe(5,1), ally(3,1)}}},
};

static const AttackReview SIGNATURE_REVIEW[] = {
    {"Breach Cut | Swap with a survivor", ATTACK_STRIKE, 0, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Breach Cut | Step into the defeated enemy's cell", ATTACK_STRIKE, 3, {c(2,1), c(3,1), {foe(3,1,3)}}},
    {"Split Shot | Side hits plus Throughshot across a gap", ATTACK_SHOOT, 4,
     {c(1,1), c(5,1), {foe(5,1), foe(5,0), foe(5,2), foe(7,1)}}},
    {"Hooking Thrust | Pull the target to the open middle", ATTACK_REACH, 0,
     {c(2,1), c(4,1), {foe(4,1)}}},
    {"Hooking Thrust | Through the Ranks swaps the ally and target", ATTACK_REACH, 3,
     {c(2,1), c(4,1), {foe(4,1), ally(3,1)}}},
    {"Wall Slam | Side hits against the board edge", ATTACK_IMPACT, 0,
     {c(6,1), c(7,1), {foe(7,1), foe(7,0), foe(7,2)}}},
    {"Feeding Frenzy | Two wounded foes and one healthy bystander", ATTACK_REND, 3,
     {c(3,1), c(4,1), {foe(4,1), foe(2,1,4), foe(3,2,4), foe(3,0)}, 8}},
    {"Whirlwind | Multiple diagonals with a Corner Sweep hit", ATTACK_ARC, 4,
     {c(3,1), c(4,2), {foe(4,2), foe(2,2), foe(2,0), foe(3,2)}}},
    {"Life Channel | Heal the other party members", ATTACK_DRAIN, 1,
     {c(2,1), c(3,1), {foe(3,1), ally(2,2,7), ally(1,1,12)}, 10}},
    {"Life Channel | A finishing hit triggers Soul Dividend", ATTACK_DRAIN, 4,
     {c(2,1), c(3,1), {foe(3,1,2), ally(2,2,7), ally(1,1,12)}, 10}},
    {"Backsweep | Sweep the two-cell retreat and drag the survivor", ATTACK_RECOIL, 4,
     {c(4,1), c(5,1), {foe(5,1), foe(3,2), foe(2,2)}}},
    {"Backsweep | Blocked retreat leaves the side enemy untouched", ATTACK_RECOIL, 4,
     {c(4,1), c(5,1), {foe(5,1), foe(3,2), ally(3,1)}}},
    {"Combined Arms | Three allies hit a durable review target together", ATTACK_FLANK, 3,
     {c(3,1), c(4,1), {foe(4,1,48), ally(4,2,14,ATTACK_FLANK,3),
                      ally(5,1,14,ATTACK_FLANK,3), ally(4,0,14,ATTACK_FLANK,3)},
      18, BATTLEFIELD_MEADOW}},
    {"Implosion | Pull two distant survivors toward the center", ATTACK_BLAST, 0,
     {c(1,1), c(4,1), {foe(4,1), foe(6,1), foe(4,3)}, 18, BATTLEFIELD_MEADOW}},
    {"Wall Slam | A tree stops the push and sends hits to both sides", ATTACK_IMPACT, 0,
     {c(4,2), c(4,1), {foe(4,1), foe(3,1), foe(5,1)}}},
    {"Implosion | The occupied midpoint prevents one pull", ATTACK_BLAST, 0,
     {c(1,1), c(4,1), {foe(4,1), foe(6,1), foe(4,3), foe(4,2)}, 18, BATTLEFIELD_MEADOW}},
};

static const AttackReview PERK_REVIEW[] = {
    {"Keen Edge, Sword Mastery and Duelist | Healthy isolated target", ATTACK_STRIKE, 3,
     {c(2,1), c(3,1), {foe(3,1)}}},
    {"Duelist comparison | A supported enemy denies its bonus", ATTACK_STRIKE, 3,
     {c(2,1), c(3,1), {foe(3,1), foe(4,1)}}},
    {"Longshot, Sharpshooter and Piercing Shot | Range four with a target behind", ATTACK_SHOOT, 3,
     {c(1,1), c(5,1), {foe(5,1), foe(6,1)}}},
    {"Throughshot | Unlimited range and a second hit across an empty tile", ATTACK_SHOOT, 4,
     {c(0,1), c(5,1), {foe(5,1), foe(7,1)}}},
    {"Short Grip | Attack an adjacent foe", ATTACK_REACH, 2, {c(2,1), c(3,1), {foe(3,1)}}},
    {"Lunge and Through the Ranks | Thrust through a friendly unit", ATTACK_REACH, 3,
     {c(2,1), c(4,1), {foe(4,1), ally(3,1)}}},
    {"Body Check, Heavy Blow and Collision | Hit the enemy behind the target", ATTACK_IMPACT, 3,
     {c(2,1), c(3,1), {foe(3,1), foe(4,1)}}},
    {"Wrecking Line | A three-enemy line slides together", ATTACK_IMPACT, 4,
     {c(2,1), c(3,1), {foe(3,1), foe(4,1), foe(5,1)}}},
    {"Bloodletting, Blood Sense and Blood Rush | Both fighters are injured", ATTACK_REND, 3,
     {c(2,1), c(3,1), {foe(3,1,7)}, 8}},
    {"Blood Hunt | Hit from range two", ATTACK_REND, 4, {c(2,1), c(4,1), {foe(4,1,4)}}},
    {"Open Angle and Crosscut | Both corner cells are open", ATTACK_ARC, 2,
     {c(2,1), c(3,2), {foe(3,2)}}},
    {"Inside Angle and Corner Sweep | Both corner cells hold enemies", ATTACK_ARC, 4,
     {c(2,1), c(3,2), {foe(3,2), foe(3,1), foe(2,2)}}},
    {"Life Tap and Deep Cut | Two HP recovered from a survivor", ATTACK_DRAIN, 2,
     {c(2,1), c(3,1), {foe(3,1)}, 12}},
    {"Deep Siphon | Three HP recovered from a survivor", ATTACK_DRAIN, 3,
     {c(2,1), c(3,1), {foe(3,1)}, 12}},
    {"Soul Dividend | A finishing blow heals the whole party", ATTACK_DRAIN, 4,
     {c(2,1), c(3,1), {foe(3,1,2), ally(2,2,8), ally(1,1,11)}, 12}},
    {"Parting Blow, Counterforce and Springback | Two-cell retreat", ATTACK_RECOIL, 3,
     {c(4,1), c(5,1), {foe(5,1)}}},
    {"Dragback | The surviving foe follows into the vacated cell", ATTACK_RECOIL, 4,
     {c(4,1), c(5,1), {foe(5,1)}}},
    {"Encircle, Tactical Edge and Converging Blades | Two supporting allies", ATTACK_FLANK, 3,
     {c(3,1), c(4,1), {foe(4,1), ally(4,2), ally(5,1)}}},
    {"Blind Side | Diagonal strike through an allied corner", ATTACK_FLANK, 4,
     {c(2,1), c(3,2), {foe(3,2), ally(3,1)}}},
    {"Cluster Burst, Shrapnel, Shockwave and Concussion | Orthogonal and diagonal splash", ATTACK_BLAST, 4,
     {c(2,1), c(5,1), {foe(5,1), foe(5,2), foe(6,2)}}},
};

static void review_tap(Device& d, uint8_t button) {
    d.advance(button, 640000);
    d.advance(0, 640000);
}

static GroveCell review_cursor(Device& d, GroveCell from, uint8_t cell) {
    GroveCell to{battle_column(cell), battle_row(cell)};
    while(from.x != to.x) {
        review_tap(d, from.x < to.x ? BTN_RIGHT : BTN_LEFT);
        from.x += from.x < to.x ? 1 : -1;
    }
    while(from.y != to.y) {
        review_tap(d, from.y < to.y ? BTN_DOWN : BTN_UP);
        from.y += from.y < to.y ? 1 : -1;
    }
    return to;
}

static void review_setup(Device& d, ReviewBoard const& board, MOVEMENT_CLASS move,
                         ATTACK_CLASS attack, uint8_t move_rank, uint8_t rank,
                         BATTLE_ATTACK_CHOICE choice, bool enemy_reply) {
    Game world = d.state();
    world.party.count = 1;
    auto& hero = world.party.members[0];
    hero.hp = board.hp;
    hero.ap = choice == BATTLE_NORMAL ? 0 : choice == BATTLE_STARTER ? 2 : 4;
    hero.level = std::max<uint8_t>(choice == BATTLE_SIGNATURE ? 5 : choice == BATTLE_STARTER ? 3 : 1,
                                  2 * std::max(move_rank, rank));
    hero.movement_class = move;
    hero.attack_class = attack;
    hero.perk_ranks = (uint8_t)(move_rank | (rank << 3));

    BattleState state{};
    state.field = board.field;
    state.turn = BATTLE_PARTY;
    state.units[state.unit_count++] = {board.from, board.hp, battle_party_id(0), BATTLE_PARTY};
    unsigned enemies = 0;
    for(auto const& unit : board.units) {
        if(unit.ally) {
            uint8_t slot = world.party.count++;
            auto& member = world.party.members[slot];
            member = hero;
            member.hp = unit.hp;
            member.movement_class = MOVEMENT_STEP;
            member.attack_class = unit.attack;
            member.perk_ranks = unit.attack_rank << 3;
            state.units[state.unit_count++] = {unit.cell, unit.hp, battle_party_id(slot), BATTLE_PARTY};
        } else {
            ++enemies;
            state.units[state.unit_count++] = {unit.cell, unit.hp, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        }
    }
    if(enemies < 2 && !enemy_reply)
        state.units[state.unit_count++] = {c(7,3), 10, BATTLE_TEST_ROGUE, BATTLE_ENEMY};
    d.set_state(world);
    d.set_battle(state);
}

static void save_review_video(FadeTrace const& trace, fs::path const& path, unsigned frames) {
    constexpr uint64_t frame_cycles = 320000;
    std::ofstream video(path, std::ios::binary);
    auto frame = trace.frames.begin();
    require(trace.frames.size() > 8, "review clip has no OLED updates");
    for(unsigned n = 0; n < frames; ++n) {
        uint64_t at = trace.frames.front().cycle + n * frame_cycles;
        while(frame + 1 != trace.frames.end() && (frame + 1)->cycle <= at)
            ++frame;
        std::array<uint8_t, 128 * 64> pixels;
        for(unsigned y = 0; y < 64; ++y)
            for(unsigned x = 0; x < 128; ++x)
                pixels[y * 128 + x] = frame->pixels[(y / 8) * 128 + x] & (1 << (y % 8)) ? 255 : 0;
        video.write(reinterpret_cast<char const*>(pixels.data()), pixels.size());
    }
    require(bool(video), "cannot write review video");
}

static void record_battle_review(Device& d, fs::path const& out) {
    start_rat_contact(d);
    settled_song(d);
    // Isolate the real game cues in the review tracks. The emulator records speaker output
    // at 16 MHz / SOUND_CYCLES from the same run used for the captured OLED frames.
    auto& cpu = d.ab->core_state.cpu;
    cpu.data[AUDIO_MUSIC_VOLUME] = 0;
    uint8_t initial = d.battle_state().units[0].cell;
    absim::arduboy_t::tt_state_t input_state{};
    d.ab->save_state_to_vector(input_state);
    require(!input_state.state.empty(), "cannot snapshot the battle input");
    const char* requested = std::getenv("RPG_BATTLE_REVIEW_GROUP");

    auto record = [&](char const* group, unsigned index, char const* title, ReviewBoard const& board,
                      MOVEMENT_CLASS move, ATTACK_CLASS attack, uint8_t move_rank, uint8_t rank,
                      BATTLE_ATTACK_CHOICE choice, bool enemy_reply) {
        if(requested && std::string_view(requested) != group)
            return;
        std::cout << "Recording " << group << ' ' << index << " - " << title << std::endl;
        d.ab->load_state_from_vector(input_state);
        avm_test::observe_spi_writes(cpu);
        d.previous_marker = cpu.data[d.marker_addr];
        review_setup(d, board, move, attack, move_rank, rank, choice, enemy_reply);
        auto cursor = review_cursor(d, {battle_column(initial), battle_row(initial)}, board.from);
        review_tap(d, BTN_A);
        review_cursor(d, cursor, board.to);
        if(choice != BATTLE_NORMAL) {
            review_tap(d, BTN_A);
            require(d.battle_state().activations == 0, "review special did not open the offer");
            review_tap(d, choice == BATTLE_SIGNATURE ? BTN_UP : BTN_DOWN);
        }

        constexpr uint64_t frame_cycles = 320000;
        constexpr unsigned before_action = 15;
        unsigned frames = enemy_reply ? 205 : 104;
        fs::path stem = out / (std::string(group) + "-" + std::to_string(index));
        cpu.sound_buffer.clear();
        FadeTrace trace(d);
        d.advance(0, before_action * frame_cycles);
        d.advance(BTN_A, 3 * frame_cycles);
        d.advance(0, (frames - before_action - 3) * frame_cycles);
        BattleState result = d.battle_state();
        require(result.activations >= 1, "review action was illegal");
        if(choice != BATTLE_NORMAL)
            require(d.state().party.members[0].ap == (choice == BATTLE_STARTER ? 0 : 1),
                    "the selected review special did not spend AP");
        if(enemy_reply) {
            uint8_t expected_hp = board.hp - (move_rank == 1 ? 3 : 1);
            if(result.activations != 2 || result.units[0].hp != expected_hp ||
               result.units[2].hp != (move_rank == 4 ? 8 : 9)) {
                std::cerr << "Defender review: activations=" << unsigned(result.activations)
                          << " defender HP=" << unsigned(result.units[0].hp)
                          << " enemy HP=" << unsigned(result.units[2].hp) << '\n';
                throw std::runtime_error("the review enemy did not trigger the expected defender perk");
            }
        }

        save_review_video(trace, stem.string() + ".gray", frames);
        unsigned rate = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;
        size_t samples = (size_t)frames * rate / 50;
        require(cpu.sound_buffer.size() >= samples, "review audio did not cover the full clip");
        std::ofstream audio(stem.string() + ".s16", std::ios::binary);
        audio.write(reinterpret_cast<char const*>(cpu.sound_buffer.data()), samples * sizeof(int16_t));
        require(bool(audio), "cannot write review audio");
        std::ofstream info(stem.string() + ".txt");
        info << title << '\n' << frames << '\n' << rate << '\n';
    };

    for(unsigned i = 0; i < std::size(MOVEMENT_REVIEW); ++i) {
        auto const& fixture = MOVEMENT_REVIEW[i];
        record("movement", i + 1, fixture.title, fixture.board, fixture.move, ATTACK_STRIKE,
               fixture.rank, 0, BATTLE_NORMAL, fixture.enemy_reply);
    }
    auto attacks = [&](char const* group, auto const& cases, BATTLE_ATTACK_CHOICE choice) {
        for(unsigned i = 0; i < std::size(cases); ++i) {
            auto const& fixture = cases[i];
            record(group, i + 1, fixture.title, fixture.board, MOVEMENT_STEP, fixture.attack, 0,
                   fixture.rank, choice, false);
        }
    };
    attacks("normal", NORMAL_REVIEW, BATTLE_NORMAL);
    attacks("starter", STARTER_REVIEW, BATTLE_STARTER);
    attacks("signature", SIGNATURE_REVIEW, BATTLE_SIGNATURE);
    attacks("perks", PERK_REVIEW, BATTLE_NORMAL);
}
