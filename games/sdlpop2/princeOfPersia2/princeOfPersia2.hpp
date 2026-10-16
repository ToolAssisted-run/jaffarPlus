#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <quickerSDLPoP2/quickerSDLPoP2.hpp>
#include "inputPruning.hpp"
#include <vector>

namespace jaffarPlus
{

namespace games
{

namespace sdlpop2
{

// Rooms 1..32 (the level format has room numbers up to 32; their tiles are at level + (room - 1) * 30)
const size_t roomCount = 32;
// The rooms with character records in the level resource (level_type.rooms: 1..28)
const size_t levelRooms = 28;

/**
 * Prince of Persia 2: The Shadow and the Flame (DOS 1.0), through the QuickerSDLPoP2 emulator.
 *
 * Game Configuration (all optional):
 *   "Hash Tile Modifiers"   [[room, tile], ...] (1-based) tiles whose modifier goes into the state hash (a gate's
 *                           height, a loose floor's state...). Modifiers are not hashed otherwise: torches and other
 *                           animations change them every tick.
 *   "Hash Random Seed"      true: the random seed goes into the state hash (guards and some traps use it)
 *   "Hash Clock"            true: the minutes and ticks left go into the state hash
 *   "Hash Visited Rooms"    [room, ...]: those rooms' visited flags ("Visited Room[r]") go into the state hash: only the ones
 *                           a rule reads (a revisit's reasons, max HP, gates, doors, tiles, are hashed already)
 *   "Hash Stood On Rows"    [[room, row], ...]: those "Stood On Row[room][row]" flags go into the state hash (the ones a rule
 *                           reads)
 *   "Hash Room Characters"  true: the level's room records (the characters of the other rooms: a head's hunt state and
 *                           spot) go into the state hash (more states: only where a route turns on them)
 *   "Restart At Checkpoint" true: with "Prune Inputs", Restart Level (the game's Alt+A, input "|R|......|") is offered while the
 *                           prince is alive in the room of the checkpoint he saved (he starts again there, standing)
 *   "Prune Inputs"          true: the game offers, for each state, one input of each group of inputs that lead to the
 *                           same game (inputPruning.hpp); the script's "Allowed Input Sets" then need no inputs
 */
class PrinceOfPersia2 final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "SDLPoP2 / Prince of Persia 2"; }

  PrinceOfPersia2(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    if (_gameConfigRemaining.contains("Hash Tile Modifiers"))
    {
      const auto tiles = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Hash Tile Modifiers");
      for (const auto& t : tiles) _hashTileModifiers.push_back({t[0].get<int>(), t[1].get<int>() - 1});
    }
    _hashRandomSeed = _gameConfigRemaining.contains("Hash Random Seed") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Random Seed");
    _hashClock      = _gameConfigRemaining.contains("Hash Clock") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Clock");
    _hashRoomChars  = _gameConfigRemaining.contains("Hash Room Characters") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Room Characters");
    if (_gameConfigRemaining.contains("Hash Visited Rooms"))
      for (const auto& r : jaffarCommon::json::popArray<int>(_gameConfigRemaining, "Hash Visited Rooms")) _hashVisited.push_back((size_t)r);
    if (_gameConfigRemaining.contains("Hash Stood On Rows"))
      for (const auto& t : jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Hash Stood On Rows")) _hashStood.push_back({t[0].get<size_t>(), t[1].get<size_t>()});
    _pruneInputs    = _gameConfigRemaining.contains("Prune Inputs") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Prune Inputs");
    _restartAtCheckpoint =
        _gameConfigRemaining.contains("Restart At Checkpoint") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Restart At Checkpoint");
    _levelEndLookahead =
        _gameConfigRemaining.contains("Level End Lookahead") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Level End Lookahead");
    _musicOnAtStart =
        _gameConfigRemaining.contains("Music On At Start") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Music On At Start");
  }

private:
  // The tiles of a room (1..32) and their modifiers, as the game addresses them
  __INLINE__ uint8_t*  roomTiles(const size_t room) const { return (uint8_t*)&_s->level + (room - 1) * 30; }
  __INLINE__ uint32_t* roomModifiers(const size_t room) const { return (uint32_t*)((uint8_t*)&_s->level + 0x348 + room * 0x78); }

  void registerCharacter(const std::string& name, quicker::char_type* c)
  {
    using dt      = Property::datatype_t;
    const auto le = Property::endianness_t::little;
    registerGameProperty(name + " Pos X", &c->x, dt::dt_int16, le);
    registerGameProperty(name + " Pos Y", &c->y, dt::dt_int16, le);
    registerGameProperty(name + " Frame", &c->frame, dt::dt_uint16, le);
    registerGameProperty(name + " Direction", &c->direction, dt::dt_int8, le);
    registerGameProperty(name + " Current Col", &c->curr_col, dt::dt_int8, le);
    registerGameProperty(name + " Current Row", &c->curr_row, dt::dt_int8, le);
    registerGameProperty(name + " Action", &c->action, dt::dt_uint8, le);
    registerGameProperty(name + " Fall Speed X", &c->fall_x, dt::dt_int8, le);
    registerGameProperty(name + " Fall Speed Y", &c->fall_y, dt::dt_int8, le);
    registerGameProperty(name + " Room", &c->room, dt::dt_uint8, le);
    registerGameProperty(name + " Char Id", &c->charid, dt::dt_uint8, le);
    registerGameProperty(name + " Is Alive", &c->alive, dt::dt_int8, le);
    registerGameProperty(name + " Current HP", &c->f12, dt::dt_uint8, le);
    registerGameProperty(name + " Max HP", &c->f13, dt::dt_uint8, le);
    registerGameProperty(name + " Sequence", &c->seq_id, dt::dt_uint16, le);
    registerGameProperty(name + " Sequence Pos", &c->seq_pos, dt::dt_uint16, le); // the place in it (a state to take an old solution up again: routegen.py --winstate)
    registerGameProperty(name + " Current Sequence", &c->f19, dt::dt_uint16, le); // the sequence started last (27: sinking in level 2's sand)
    registerGameProperty(name + " Sword Drawn", &c->f10, dt::dt_uint8, le);       // 1: sword drawn (0xFF: none to draw)
  }

  __INLINE__ void registerGameProperties() override
  {
    using dt      = Property::datatype_t;
    const auto le = Property::endianness_t::little;

    // The core's savestate, in place (it changes as the emulator's state does)
    _s = (quicker::QuickerSDLPoP2State*)_emulator->getProperty("Game State").pointer;

    registerGameProperty("Current Level", &_s->level_number, dt::dt_uint8, le);
    registerGameProperty("Next Level", &_s->counter_5cec, dt::dt_uint16, le);
    registerGameProperty("Level End Wait", &_levelEndWait, dt::dt_uint16, le);   // (updateLevelEndWait)
    registerGameProperty("Drawn Room", &_s->drawn_room, dt::dt_uint8, le);
    registerGameProperty("Minutes Left", &_s->minutes_left, dt::dt_uint16, le);
    registerGameProperty("Clock Ticks", &_s->clock_ticks, dt::dt_uint16, le);
    registerGameProperty("Random Seed", &_s->random_seed, dt::dt_uint32, le);
    registerGameProperty("Tick", &_s->tick, dt::dt_uint32, le);
    registerGameProperty("Sword Type", &_s->byte_5cba, dt::dt_uint8, le);
    // the standing turns in a row on the temple and final levels (spirit.c turn_count: the eighth leaves the body; a
    // standing frame clears it)
    registerGameProperty("Turn Count", &_s->word_5cbe, dt::dt_uint16, le);
    registerGameProperty("Restart Requested", &_s->word_5cd8, dt::dt_uint16, le);
    registerGameProperty("Moving Object Count", &_s->mob_count, dt::dt_uint16, le);
    registerGameProperty("Sea Grabbed Char", &_s->byte_9276, dt::dt_uint8, le); // level 1's sea: the character it took (10 the prince, 255 none)
    registerGameProperty("Puzzle Answer", &_s->puzzle_answer, dt::dt_int8, le);  // level 2: the sand tile (0..5) that must stay up
    registerGameProperty("Puzzle Chosen", &_s->byte_14a0, dt::dt_uint8, le);     // level 2: 255 until room 1 is entered
    registerGameProperty("Checkpoint Saved", &_s->cp.index, dt::dt_uint16, le);  // the checkpoint saved (1, 2; 0 none, and see cp.used)
    registerGameProperty("Tile Animation Count", &_s->trob_count, dt::dt_uint16, le);
    registerGameProperty("Player Smooth Pos Y", &_kidPosY, dt::dt_float32, le);   // y smoothed through climbs (stateUpdatePostHook)
    for (size_t r = 1; r <= roomCount; r++) registerGameProperty("Visited Room[" + std::to_string(r) + "]", &_visited[r], dt::dt_uint8, le);
    // "Stood On Row[room][row]": he has stood (frame 15) on that row of that room (latched, as the visited flags)
    for (size_t r = 1; r <= roomCount; r++)
      for (size_t w = 0; w < 3; w++)
      {
        registerGameProperty("Stood On Row[" + std::to_string(r) + "][" + std::to_string(w) + "]", &_stood[r][w], dt::dt_uint8, le);
        registerGameProperty("HP When Stood[" + std::to_string(r) + "][" + std::to_string(w) + "]", &_stoodHP[r][w], dt::dt_uint8, le);   // (his hit points then)
      }
    // "Room Heads Hunting[room]": its floating heads (charids 7/8, types 5/6) alive and hunting or biting (their record's state,
    // heads.c; the room he is in counts its live characters' hit points), as stateUpdatePostHook counts them
    for (size_t r = 1; r <= levelRooms; r++) registerGameProperty("Room Heads Hunting[" + std::to_string(r) + "]", &_headsHunting[r], dt::dt_uint8, le);
    // The prince's room (stateUpdatePostHook): its big potions (tile 0x0A, kind 2: a point of maximum hit points) and
    // its other potions; its guards (charid 2) alive; "Kill Balance", its guards dead for good (alive >= 7: past the
    // tick that may leave a potion where they fell, level 14's room 3) less its big potions: the kills that left none
    // (and the potions drunk since, while their bodies lie)
    // "Room Chars[r]": the characters in room r's list (the level's own count: it outlives a search's start, unlike the
    // kill counters; level 14's room 6: four Jaffars, and the spirit's body while he is there)
    for (size_t r = 1; r <= roomCount; r++) registerGameProperty("Room Chars[" + std::to_string(r) + "]", &_s->level.rooms[r - 1].nchars, dt::dt_uint8, le);
    registerGameProperty("Room Big Potions", &_roomBigPotions, dt::dt_uint8, le);
    registerGameProperty("Room Other Potions", &_roomOtherPotions, dt::dt_uint8, le);
    registerGameProperty("Room Live Guards", &_roomLiveGuards, dt::dt_uint8, le);
    registerGameProperty("Kill Balance", &_killBalance, dt::dt_int8, le);
    // "Room Guards Damage": the hit points its live guards have lost, together; "Guard Kills": the guards that died in
    // the prince's room since the search began (counted as they die: a body gone later does not take it back)
    registerGameProperty("Room Guards Damage", &_roomGuardsDamage, dt::dt_uint8, le);
    // "Room Live Guards On Row": those of them on the prince's row (level 14: the spirit's fireball flies along his row)
    registerGameProperty("Room Live Guards On Row", &_roomGuardsOnRow, dt::dt_uint8, le);
    registerGameProperty("Guard Kills", &_guardKills, dt::dt_uint8, le);
    // "Wasted Kills": of those, the ones that left no big potion where they fell (seen on the tick the body is dead for
    // good, alive 7: level 14's room 3 draws what it leaves a tick before)
    registerGameProperty("Wasted Kills", &_wastedKills, dt::dt_uint8, le);
    registerGameProperty("Bridge Count", &_s->word_693c, dt::dt_int16, le);       // level 5's rope bridge: ticks the prince and room 10's character stood together
    registerGameProperty("Bridge State", &_s->bridge_693e[0], dt::dt_int16, le);  // 0 waiting, 1..3 starting, -1 collapsed

    registerCharacter("Player", &_s->Kid);
    registerCharacter("Opponent", &_s->Opp);
    for (size_t i = 0; i < 5; i++) registerCharacter("Character[" + std::to_string(i) + "]", &_s->chars[i]);

    // Tiles and their modifiers: "Tile[room][tile]", "Tile Modifier[room][tile]" (1-based); "Tile Opening[room][tile]",
    // the modifier's low byte: a door's or gate's opening (level 6's doors carry 0x2080000 above it, shut or open)
    for (size_t r = 1; r <= roomCount; r++)
      for (size_t t = 1; t <= 30; t++)
      {
        const auto idx = "[" + std::to_string(r) + "][" + std::to_string(t) + "]";
        registerGameProperty("Tile" + idx, &roomTiles(r)[t - 1], dt::dt_uint8, le);
        registerGameProperty("Tile Modifier" + idx, &roomModifiers(r)[t - 1], dt::dt_uint32, le);
        registerGameProperty("Tile Opening" + idx, (uint8_t*)&roomModifiers(r)[t - 1], dt::dt_uint8, le);
      }

    // Falling floors and other moving objects
    for (size_t i = 0; i < 30; i++)
    {
      const auto idx = "Moving Object[" + std::to_string(i) + "]";
      registerGameProperty(idx + " Pos X", &_s->mobs[i].x, dt::dt_int16, le);
      registerGameProperty(idx + " Pos Y", &_s->mobs[i].y, dt::dt_int16, le);
      registerGameProperty(idx + " Room", &_s->mobs[i].room, dt::dt_uint8, le);
      registerGameProperty(idx + " Type", &_s->mobs[i].type, dt::dt_uint8, le);
      registerGameProperty(idx + " Row", &_s->mobs[i].row, dt::dt_uint8, le);
    }

    // Tile animations (gates, buttons, loose floors, ...)
    for (size_t i = 0; i < 20; i++)
    {
      const auto idx = "Tile Animation[" + std::to_string(i) + "]";
      registerGameProperty(idx + " Tile", &_s->trobs[i].tilepos, dt::dt_int8, le);
      registerGameProperty(idx + " Room", &_s->trobs[i].room, dt::dt_uint8, le);
      registerGameProperty(idx + " State", &_s->trobs[i].state, dt::dt_uint8, le);
      registerGameProperty(idx + " Type", &_s->trobs[i].tile, dt::dt_uint8, le);
    }

    _nullInputIdx = _emulator->registerInput("|.|......|");

    // Every input, simplest first (the one offered for a group of inputs is its first): no keystroke, no Shift / Ctrl,
    // the fewest directions
    if (_pruneInputs)
    {
      std::vector<pop2Input> inputs;
      for (int8_t k = 0; k < 2; k++)
        for (int8_t sh = 0; sh < 3; sh++)
          for (int8_t n = 0; n < 3; n++)
            for (int8_t y = -1; y <= 1; y++)
              for (int8_t x = -1; x <= 1; x++)
                if ((x != 0) + (y != 0) == n) inputs.push_back({x, y, sh, k});
      for (const auto& in : inputs)
      {
        std::string str = std::string("|") + (in.keystroke ? 'K' : '.') + "|" + (in.x < 0 ? 'L' : '.') + (in.x > 0 ? 'R' : '.') + (in.y < 0 ? 'U' : '.') +
                          (in.y > 0 ? 'D' : '.') + (in.shift == 1 ? 'S' : '.') + (in.shift == 2 ? 'C' : '.') + "|";
        _prunedInputs.push_back({in, _emulator->registerInput(str)});
        // "Music On At Start": the same input with the game's Music On/Off key (Alt+M) in the key's place
        if (_musicOnAtStart)
        {
          if (in.keystroke == 0) str[1] = 'M';
          _musicInputs.push_back(in.keystroke == 0 ? _emulator->registerInput(str) : _prunedInputs.back().second);
        }
      }
      if (_restartAtCheckpoint) _restartInputIdx = _emulator->registerInput("|R|......|");
    }
  }

  // With "Prune Inputs": every input the game may offer (so the runner registers them and sizes its input history)
  __INLINE__ std::set<std::string> getAllPossibleInputs() override
  {
    std::set<std::string> inputs;
    for (const auto& p : _prunedInputs) inputs.insert(_emulator->getRegisteredInput(p.second).inputString);
    for (const auto& i : _musicInputs) inputs.insert(_emulator->getRegisteredInput(i).inputString);
    if (_restartAtCheckpoint) inputs.insert("|R|......|");
    return inputs;
  }

  // With "Prune Inputs": one input of each group of inputs that lead to the same game
  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override
  {
    if (!_pruneInputs) return;
    InputPruner           pruner(_s);
    uint64_t              keys[64];
    size_t                nKeys = 0;
    // "Music On At Start": the run switches the ambient music off to end a level's wait for its music (the game's
    // Alt+M during the level-end music loads the next level at once) and on again on the next level's first tick. So
    // the key is the one input offered on two kinds of tick: with the music on, while the level-end music plays
    // (its first tick: the key stops it, and the search plays the level's end as the run does: what is left of the
    // wait is the effect playing, and the win is the next level loaded), and with the music off outside a level's
    // end (the next level's first tick, where a search starts; the prince inside the door is frame 0).
    // The key alone: it is Alt+M, and with Alt held the arrows are not the prince's (Chimera's core, a keyboard as
    // DOS has it, drops a direction pressed with it; this core's input would take both)
    bool music = false;
    if (_musicOnAtStart)
    {
      const int16_t piece    = _s->snd_ch[1].id;
      const bool    endMusic = (piece == 0x1C || piece == 0x1D || piece == 0x1E) && (int32_t)(_s->snd_ch[1].end - _s->snd_time) > 0;
      const bool    levelEnd = _s->Kid.frame == 0 || (_s->Kid.frame >= 0xD9 && _s->Kid.frame <= 0xE2) || (int8_t)_s->word_32d8 != (int16_t)_s->counter_5cec;
      music                  = _s->amb_state[0] != 0 ? endMusic : !levelEnd;
    }
    if (music)
    {
      for (size_t j = 0; j < _prunedInputs.size(); j++)
      {
        const auto& in = _prunedInputs[j].first;
        if (in.x == 0 && in.y == 0 && in.shift == 0 && in.keystroke == 0) allowedInputSet.push_back(_musicInputs[j]);
      }
      return;
    }
    for (size_t j = 0; j < _prunedInputs.size(); j++)
    {
      const auto& p    = _prunedInputs[j];
      const auto  k    = pruner.key(p.first);
      bool        seen = false;
      for (size_t i = 0; i < nKeys && !seen; i++) seen = keys[i] == k;
      if (seen) continue;
      keys[nKeys++] = k;
      allowedInputSet.push_back(p.second);
    }

    // Restart Level at the checkpoint: the whole state is reloaded from it, so it needs no other input
    if (_restartAtCheckpoint && _s->cp.used && (int8_t)_s->Kid.alive < 0)
    {
      const uint8_t* table = (const uint8_t*)&_s->level + 0x26E7; // level.c checkpoint_table: two (room, tile) pairs
      if (_s->cp.index >= 1 && _s->cp.index <= 2 && _s->Kid.room == table[(_s->cp.index - 1) * 2]) allowedInputSet.push_back(_restartInputIdx);
    }
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override { _emulator->advanceState(input); }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    hashEngine.Update(_s->level_number);
    hashEngine.Update(_s->counter_5cec);
    hashEngine.Update(_s->drawn_room);
    hashEngine.Update(_s->word_5cd8);
    hashEngine.Update(_s->byte_5cba);
    hashEngine.Update(_s->Kid);
    // The prince's press states between ticks: a jump or a grab needs a fresh press
    hashEngine.Update(_s->kid_ctrl1_saved);
    hashEngine.Update(_s->word_8a84);
    hashEngine.Update(_s->word_5cbe);   // the turns in a row (the spirit's)
    hashEngine.Update(_s->word_5d38);
    hashEngine.Update(_s->puzzle_answer); // level 2's puzzle
    hashEngine.Update(_s->puzzle_last);
    hashEngine.Update(_s->byte_14a0);
    hashEngine.Update(_s->word_693c);   // level 5's rope bridge: its collapse count and state
    hashEngine.Update(_s->bridge_693e);
    hashEngine.Update(_s->cp.used); // the checkpoint a restart starts from
    hashEngine.Update(_s->cp.index);
    hashEngine.Update(_s->Opp);
    hashEngine.Update(_s->chars);
    hashEngine.Update(_s->mob_count);
    hashEngine.Update(_s->mobs);
    hashEngine.Update(_s->trob_count);
    hashEngine.Update(_s->trobs);
    // the running tile animations keep their state in the tiles' modifiers (a slicer blade's phase, blades.c): the
    // modifiers of each animating room (else states a blade's phase apart are merged, and the search loses timings)
    for (size_t i = 0; i < _s->trob_count && i < 20; i++)
      if (_s->trobs[i].room >= 1 && _s->trobs[i].room <= roomCount) hashEngine.Update((const uint8_t*)roomModifiers(_s->trobs[i].room), 30 * sizeof(uint32_t));
    // the characters of the rooms he is not in live in the level's room records (a head's hunt state and spot, a
    // guard's hit points): else states apart only there are merged (level 9: room 8's heads woken on the way)
    if (_hashRoomChars) hashEngine.Update((const uint8_t*)&_s->level.rooms, sizeof(_s->level.rooms));
    for (size_t r = 1; r <= roomCount; r++) hashEngine.Update(roomTiles(r), 30);
    for (const auto& t : _hashTileModifiers) hashEngine.Update(roomModifiers(t.first)[t.second]);
    if (_hashRandomSeed) hashEngine.Update(_s->random_seed);
    for (const auto r : _hashVisited) hashEngine.Update(_visited[r]);
    for (const auto& t : _hashStood) hashEngine.Update(_stood[t.first][t.second]);
    // entering the level door (sequence 0x46): the prince stands still inside it until the level-end sound stops, nothing
    // else hashed changing; the clock keeps those ticks apart (else a wait inside it is a repeat, and dropped)
    if (_hashClock || _s->Kid.f19 == 0x46)
    {
      hashEngine.Update(_s->minutes_left);
      hashEngine.Update(_s->clock_ticks);
    }
  }

  // The prince's y for the Y magnet, smoothed through climbs (as in the PoP1 game): his y changes only once in a climb
  // (63 px, mid-animation) and not at all in a jump-up or a hang, so a magnet on it rewards waiting as much as climbing.
  // The frames move it a little at a time instead (PoP2's sequences, SDLPoP2 docs/SEQUENCE_DUMP.txt):
  //   jump up and grab (seqs 8, 16, 24)  f67..f80   1 px up a frame
  //   hanging (seqs 9, 25, 210)          f87..f99   20 px up
  //   climb up (seq 10 -> 235, 73)       f135..f140 24..44 px up, then (63 px higher from f141) f141..f149 17..1 px down
  //   climb down (seq 68)                f148..f141 0..14 px down, then (63 px lower from f140) f140..f136 45..35 px up
  // The level's end: once the next level is decided ("Next Level" differs from the level played) the game still waits
  // for the level-end music, and any other music piece started in the meantime ends that wait (SDLPoP2 sound.c; so does
  // DOS: a piece queued in level 1's wait loads level 2 on the next tick). From the door's stairs on (frames 0xD9..0xE2)
  // the prince can do nothing, so the rest is read off the game itself: the ticks played ahead without input until the
  // next level loads ("Level End Wait": 65535 before that, or without "Level End Lookahead" in the game's
  // configuration; 400 when it does not load, e.g. the prince hit on the stairs)
  __INLINE__ void updateLevelEndWait()
  {
    _levelEndWait = 65535;
    if (_levelEndLookahead == false) return;
    if ((int8_t)_s->word_32d8 == (int16_t)_s->counter_5cec && (_s->Kid.frame < 0xD9 || _s->Kid.frame > 0xE2)) return;
    if (_lookaheadBuffer.size() != _emulator->getStateSize()) _lookaheadBuffer.resize(_emulator->getStateSize());
    jaffarCommon::serializer::Contiguous s(_lookaheadBuffer.data(), _lookaheadBuffer.size());
    _emulator->serializeState(s);
    const uint8_t level = _s->level_number;
    uint16_t      wait  = 0;
    while (_s->level_number == level && wait < 400)
    {
      _emulator->advanceState(_nullInputIdx);
      wait++;
    }
    jaffarCommon::deserializer::Contiguous d(_lookaheadBuffer.data(), _lookaheadBuffer.size());
    _emulator->deserializeState(d);
    _levelEndWait = wait;
  }

  __INLINE__ void stateUpdatePostHook() override
  {
    updateLevelEndWait();
    const int f = _s->Kid.frame;
    float     o = 0.0f;
    if (f >= 67 && f <= 80) o = -(float)(f - 66);
    else if (f >= 87 && f <= 99) o = -20.0f;
    else if (_s->Kid.f19 == 68 && f >= 141 && f <= 148) o = 2.0f * (float)(148 - f);
    else if (_s->Kid.f19 == 68 && f >= 136 && f <= 140) o = -45.0f + 2.5f * (float)(140 - f);
    else if (f >= 135 && f <= 140) o = -(20.0f + 4.0f * (float)(f - 134));
    else if (f >= 141 && f <= 149) o = 19.0f - 2.0f * (float)(f - 140);
    _kidPosY = (float)_s->Kid.y + o;
    if (_s->Kid.room >= 1 && _s->Kid.room <= roomCount) _visited[_s->Kid.room] = 1;
    if (_s->Kid.room >= 1 && _s->Kid.room <= roomCount && _s->Kid.frame == 15 && _s->Kid.curr_row >= 0 && _s->Kid.curr_row < 3 && _stood[_s->Kid.room][_s->Kid.curr_row] == 0)
    {
      _stood[_s->Kid.room][_s->Kid.curr_row]   = 1;
      _stoodHP[_s->Kid.room][_s->Kid.curr_row] = _s->Kid.f12;
    }
    for (size_t r = 1; r <= levelRooms; r++)
    {
      const auto& room = _s->level.rooms[r - 1];
      uint8_t     n    = 0;
      for (size_t i = 0; i < room.nchars && i < 5; i++)
      {
        const auto&   c  = room.chars[i];
        if (c.type != 5 && c.type != 6) continue;   // (the heads' types: DS:0096 maps types 5, 6 to charids 7, 8)
        const uint8_t hp = _s->Kid.room == r ? _s->chars[i].f12 : c.hp;
        const uint8_t st = (uint8_t)c.y;   // (the low byte: the head's state, 1 hunting, 3 biting)
        if (hp > 0 && (st == 1 || st == 3)) n++;
      }
      _headsHunting[r] = n;
    }
    // the prince's room: potions, guards (see the properties)
    _roomBigPotions = _roomOtherPotions = _roomLiveGuards = 0;
    _killBalance    = 0;
    _bigPotionDist  = 0.0f;
    const uint8_t kr = _s->Kid.room;
    if (kr >= 1 && kr <= roomCount)
    {
      float best = 1.0e9f;
      for (int t = 0; t < 30; t++)
        if (roomTiles(kr)[t] == 0x0A)
        {
          if (((roomModifiers(kr)[t] >> 5) & 7) != 2) { _roomOtherPotions++; continue; }
          _roomBigPotions++;
          // (column c spans x 146 + 32 c .. 177 + 32 c: its middle; row r's floor is at y 56 + 63 r: his y smoothed
          // through climbs, so that a climb towards it counts all along)
          const float d = std::abs((float)_s->Kid.x - (float)(162 + 32 * (t % 10))) + std::abs(_kidPosY - (float)(56 + 63 * (t / 10)));
          if (d < best) best = d;
        }
      if (_roomBigPotions > 0) _bigPotionDist = best;
      int        bodies = 0;
      float      near   = 1.0e9f, nearRow = 1.0e9f;
      _roomGuardsOnRow  = 0;
      const auto n      = _s->level.rooms[kr - 1].nchars;
      _roomGuardsDamage = 0;
      for (size_t i = 0; i < 5; i++)
      {
        // (guards: charid 2, and level 14's Jaffars, charid 6: his room 6 illusions die of one cut)
        const bool   guard = i < n && (_s->chars[i].charid == 2 || _s->chars[i].charid == 6);
        const int8_t alive = guard ? (_s->chars[i].alive < 0 ? -1 : 1) : 0;   // (-1 alive, 1 dead, 0 no guard in the slot)
        if (guard && alive < 0)
        {
          _roomLiveGuards++;
          _roomGuardsDamage += (uint8_t)std::max(0, (int)_s->chars[i].f13 - (int)_s->chars[i].f12);
          const float d = std::abs((float)_s->Kid.x - (float)_s->chars[i].x) + std::abs(_kidPosY - (float)_s->chars[i].y);
          if (d < near) near = d;
          const float dr = std::abs(_kidPosY - (float)_s->chars[i].y);
          if (dr < nearRow) nearRow = dr;
          if (_s->chars[i].curr_row == _s->Kid.curr_row) _roomGuardsOnRow++;
        }
        else if (guard && _s->chars[i].charid == 2 && _s->chars[i].alive >= 7) bodies++;
        // a guard of this slot alive on the last tick (in this room) and dead now: a kill
        if (_prevRoom == kr && _prevAlive[i] == -1 && alive == 1 && _guardKills < 255) _guardKills++;
        _prevAlive[i] = alive;
        // ... and dead for good since this tick: what did he leave?
        const uint8_t decided = guard && _s->chars[i].charid == 2 && _s->chars[i].alive >= 7 ? 1 : 0;
        if (_prevRoom == kr && decided && !_prevDecided[i] && alive == 1)
        {
          const int tp = (int)_s->chars[i].curr_row * 10 + (int)_s->chars[i].curr_col;
          const bool big = tp >= 0 && tp < 30 && roomTiles(kr)[tp] == 0x0A && ((roomModifiers(kr)[tp] >> 5) & 7) == 2;
          if (!big && _wastedKills < 255) _wastedKills++;
        }
        _prevDecided[i] = decided;
      }
      // level 14's fireball (moving object type 0xC) to the nearest live guard (Jaffar): x apart, a row counting 63
      // (rooms 7 | 8 as one: the characters in the drawn room's coordinates, the fireball in its own room's, 0x71 right
      // of its x as final.c's hit test takes it); 0 with no fireball or no one to hit
      _fireballDist = 0.0f;
      if (_roomLiveGuards > 0)
      {
        float bestF = 1.0e9f;
        for (size_t m = 0; m < _s->mob_count && m < 30; m++)
          if (_s->mobs[m].type == 0x0C && _s->mobs[m].speed >= 0)
          {
            const float fx = (float)_s->mobs[m].x + 113.0f + (_s->mobs[m].room == 8 ? 320.0f : 0.0f);
            for (size_t i = 0; i < n && i < 5; i++)
              if ((_s->chars[i].charid == 2 || _s->chars[i].charid == 6) && _s->chars[i].alive < 0)
              {
                const float gx = (float)_s->chars[i].x + (_s->drawn_room == 8 ? 320.0f : 0.0f);
                const float d  = std::abs(fx - gx) + 63.0f * std::abs((float)((int)_s->mobs[m].row - (int)_s->chars[i].curr_row));
                if (d < bestF) bestF = d;
              }
          }
        if (bestF < 1.0e8f) _fireballDist = bestF;
      }
      _guardDist   = _roomLiveGuards > 0 ? near : 0.0f;
      _guardRowDist = _roomLiveGuards > 0 ? nearRow : 0.0f;
      _killBalance = (int8_t)(bodies - (int)_roomBigPotions);
    }
    else
    {
      _guardDist = 0.0f; _guardRowDist = 0.0f; _roomGuardsDamage = 0; _roomGuardsOnRow = 0; _fireballDist = 0.0f;
      for (size_t i = 0; i < 5; i++) { _prevAlive[i] = 0; _prevDecided[i] = 0; }
    }
    _prevRoom = kr;
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    _playerPosXMagnet.intensity = 0.0;
    _playerPosYMagnet.intensity = 0.0;
    _playerDirectionMagnet      = 0.0;
    _opponentHPMagnet           = 0.0;
    _playerHPMagnet             = 0.0;
    _opponentPosXMagnet.intensity = 0.0;
    _opponentPosYMagnet.intensity = 0.0;
    _puzzleMagnet               = 0.0;
    _headsMagnet.intensity      = 0.0;
    _bigPotionMagnet            = 0.0;
    _guardMagnet                = guardMagnet_t{};
    _fireballMagnet             = 0.0;
  }

  // The rooms visited so far (since the search began): part of the state, so a route can ask for a room to have been
  // passed ("Visited Room[r]" == 1), e.g. room 9 before room 7
  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(_visited, sizeof(_visited));
    serializer.push(_stood, sizeof(_stood));
    serializer.push(_stoodHP, sizeof(_stoodHP));
    serializer.push(&_guardKills, sizeof(_guardKills));
    serializer.push(_prevAlive, sizeof(_prevAlive));
    serializer.push(&_prevRoom, sizeof(_prevRoom));
    serializer.push(&_wastedKills, sizeof(_wastedKills));
    serializer.push(_prevDecided, sizeof(_prevDecided));
  }
  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(_visited, sizeof(_visited));
    deserializer.pop(_stood, sizeof(_stood));
    deserializer.pop(_stoodHP, sizeof(_stoodHP));
    deserializer.pop(&_guardKills, sizeof(_guardKills));
    deserializer.pop(_prevAlive, sizeof(_prevAlive));
    deserializer.pop(&_prevRoom, sizeof(_prevRoom));
    deserializer.pop(&_wastedKills, sizeof(_wastedKills));
    deserializer.pop(_prevDecided, sizeof(_prevDecided));
  }

  __INLINE__ float calculateGameSpecificReward() const override
  {
    float reward = 0.0;
    reward += _playerPosXMagnet.intensity * -std::abs((float)_playerPosXMagnet.position - (float)_s->Kid.x);
    reward += _playerPosYMagnet.intensity * -std::abs((float)_playerPosYMagnet.position - _kidPosY);   // (smoothed: see stateUpdatePostHook)
    reward += (_s->Kid.direction == 0 ? 1.0f : -1.0f) * _playerDirectionMagnet;
    reward += (float)((int)_s->Opp.f13 - (int)_s->Opp.f12) * _opponentHPMagnet;
    reward += (float)_s->Kid.f12 * _playerHPMagnet;
    // (no opponent in the slot: as far as it gets, so that losing the opponent never pays)
    reward += _opponentPosXMagnet.intensity * (_s->Opp.room != 0 ? -std::abs((float)_opponentPosXMagnet.position - (float)_s->Opp.x) : -1000.0f);
    reward += _opponentPosYMagnet.intensity * (_s->Opp.room != 0 ? -std::abs((float)_opponentPosYMagnet.position - (float)_s->Opp.y) : -1000.0f);
    if (_puzzleMagnet != 0.0f) reward += _puzzleMagnet * puzzleProgress();
    if (_headsMagnet.intensity != 0.0f) reward += _headsMagnet.intensity * -headsDistance(_headsMagnet.room, _headsMagnet.x, _headsMagnet.row);
    reward += _bigPotionMagnet * -_bigPotionDist;   // (no big potion in his room: nothing)
    reward += _fireballMagnet * -_fireballDist;   // (Sergio: the fireball's way to Jaffar)
    // the guards of his room (level 14's room 3, where two may stand): the hit points the live ones have lost, the
    // kills so far (a kill must outweigh the damage it wipes: Kill Intensity above 5 Damage Intensity), and the way
    // to the nearest live one
    reward += _guardMagnet.damage * (float)_roomGuardsDamage + _guardMagnet.kill * (float)_guardKills - _guardMagnet.distance * _guardDist - _guardMagnet.row * _guardRowDist;
    return reward;
  }

  // A room's live floating heads (types 5/6): the sum of their distances to (x, row), a row counting 63 (the room he is
  // in: its live characters; any other: their records, as left when he went out)
  __INLINE__ float headsDistance(const uint8_t r, const int16_t x, const int8_t row) const
  {
    if (r < 1 || r > levelRooms) return 0.0f;
    const auto& room = _s->level.rooms[r - 1];
    float       d    = 0.0f;
    for (size_t i = 0; i < room.nchars && i < 5; i++)
    {
      const auto& c = room.chars[i];
      if (c.type != 5 && c.type != 6) continue;
      const bool    in = _s->Kid.room == r;
      const uint8_t hp = in ? _s->chars[i].f12 : c.hp;
      if (hp == 0) continue;
      const int16_t hx = in ? _s->chars[i].x : c.x;
      const int8_t  hr = in ? _s->chars[i].curr_row : (int8_t)(c.tilepos / 10);
      d += std::abs((float)(hx - x)) + 63.0f * std::abs((float)(hr - row));
    }
    return d;
  }

  void printInfoImpl() const override
  {
    const auto& k = _s->Kid;
    jaffarCommon::logger::log("[J+]  + Level / Next Level:   %2u / %2u (level end wait: %u)\n", _s->level_number, _s->counter_5cec, _levelEndWait);
    jaffarCommon::logger::log("[J+]  + Time Left:            %u minutes, %u ticks\n", _s->minutes_left, _s->clock_ticks);
    jaffarCommon::logger::log("[J+]  + [Prince]              Room: %u, Pos: %d, %d (smooth %.0f), Row: %d, Col: %d, Frame: 0x%X, Action: %u, Dir: %d, HP: %u/%u, Alive: %d\n", k.room,
                              k.x, k.y, _kidPosY, k.curr_row, k.curr_col, k.frame, k.action, k.direction, k.f12, k.f13, k.alive);
    const auto& o = _s->Opp;
    if (o.room != 0)
      jaffarCommon::logger::log("[J+]  + [Opponent]            Room: %u, Pos: %d, %d, Frame: 0x%X, Char Id: %u, HP: %u/%u, Alive: %d\n", o.room, o.x, o.y, o.frame, o.charid, o.f12,
                                o.f13, o.alive);
    if (_roomLiveGuards != 0 || _guardKills != 0 || _roomBigPotions != 0 || _roomOtherPotions != 0)
      jaffarCommon::logger::log("[J+]  + [Room]                Live Guards: %u (damage %u, nearest %.0f), Kills: %u (wasted %u), Big Potions: %u (nearest %.0f), Other Potions: %u, Fireball: %.0f\n",
                                _roomLiveGuards, _roomGuardsDamage, _guardDist, _guardKills, _wastedKills, _roomBigPotions, _bigPotionDist, _roomOtherPotions, _fireballDist);
    jaffarCommon::logger::log("[J+]  + Random Seed:          0x%08X\n", _s->random_seed);
    for (size_t i = 0; i < _s->mob_count && i < 30; i++)
      jaffarCommon::logger::log("[J+]  + Moving Object %lu:      Room: %u, Pos: %d, %d, Type: %u, Row: %u\n", i, _s->mobs[i].room, _s->mobs[i].x, _s->mobs[i].y, _s->mobs[i].type,
                                _s->mobs[i].row);
    for (size_t i = 0; i < _s->trob_count && i < 20; i++)
      jaffarCommon::logger::log("[J+]  + Tile Animation %lu:     Room: %u, Tile: %d, Type: 0x%02X, State: %u\n", i, _s->trobs[i].room, _s->trobs[i].tilepos + 1, _s->trobs[i].tile,
                                _s->trobs[i].state);
    if (std::abs(_playerPosXMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos X Magnet   - Intensity: %.1f, Center: %d\n", _playerPosXMagnet.intensity, _playerPosXMagnet.position);
    if (std::abs(_playerPosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos Y Magnet   - Intensity: %.1f, Center: %d\n", _playerPosYMagnet.intensity, _playerPosYMagnet.position);
    if (std::abs(_playerDirectionMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Player Direction Magnet - Intensity: %.1f\n", _playerDirectionMagnet);
    if (std::abs(_opponentHPMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Opponent HP Magnet    - Intensity: %.1f\n", _opponentHPMagnet);
    if (std::abs(_playerHPMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Player HP Magnet      - Intensity: %.1f\n", _playerHPMagnet);
    if (std::abs(_opponentPosXMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Opponent Pos X Magnet - Intensity: %.1f, Center: %d\n", _opponentPosXMagnet.intensity, _opponentPosXMagnet.position);
    if (_headsMagnet.intensity != 0.0f)
      jaffarCommon::logger::log("[J+]  + Room Heads Magnet - Room: %u, Intensity: %.1f, Spot: %d row %d, Distance: %.0f\n", _headsMagnet.room, _headsMagnet.intensity,
                                _headsMagnet.x, _headsMagnet.row, headsDistance(_headsMagnet.room, _headsMagnet.x, _headsMagnet.row));
    if (std::abs(_opponentPosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Opponent Pos Y Magnet - Intensity: %.1f, Center: %d\n", _opponentPosYMagnet.intensity, _opponentPosYMagnet.position);
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    // "Set Player Pos X Magnet" / "Set Player Pos Y Magnet": Intensity, Position, Room (the magnet only acts in that room)
    if (actionType == "Set Player Pos X Magnet" || actionType == "Set Player Pos Y Magnet")
    {
      auto  intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto  position  = jaffarCommon::json::getNumber<int16_t>(actionJs, "Position");
      auto  room      = jaffarCommon::json::getNumber<uint8_t>(actionJs, "Room");
      auto& magnet    = actionType == "Set Player Pos X Magnet" ? _playerPosXMagnet : _playerPosYMagnet;
      rule.addAction(
          [=, this, &magnet]()
          {
            if (_s->Kid.room == room) magnet = pointMagnet_t{.intensity = intensity, .position = position};
          });
      return true;
    }
    if (actionType == "Set Player Direction Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _playerDirectionMagnet = intensity; });
      return true;
    }
    // "Set Puzzle Magnet": Intensity (level 2's sand puzzle: see puzzleProgress)
    if (actionType == "Set Puzzle Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _puzzleMagnet = intensity; });
      return true;
    }
    if (actionType == "Set Opponent HP Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _opponentHPMagnet = intensity; });
      return true;
    }
    // "Set Opponent Pos X Magnet" / "Set Opponent Pos Y Magnet": Intensity, Position (the opponent slot's x / y, while it
    // holds one: e.g. a level 7 head to be drawn next to the prince, to push him through a wall or gate)
    if (actionType == "Set Opponent Pos X Magnet" || actionType == "Set Opponent Pos Y Magnet")
    {
      auto  intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto  position  = jaffarCommon::json::getNumber<int16_t>(actionJs, "Position");
      auto& magnet    = actionType == "Set Opponent Pos X Magnet" ? _opponentPosXMagnet : _opponentPosYMagnet;
      rule.addAction([=, this, &magnet]() { magnet = pointMagnet_t{.intensity = intensity, .position = position}; });
      return true;
    }
    // "Set Big Potion Magnet": Intensity: draws the prince to the nearest big potion of his room (x distance to its
    // tile's middle and to its row's floor, his y smoothed through climbs; nothing while the room has none)
    if (actionType == "Set Big Potion Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _bigPotionMagnet = intensity; });
      return true;
    }
    // "Set Fireball Magnet": Intensity per pixel between the fireball and the nearest live guard (a row counting 63)
    if (actionType == "Set Fireball Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _fireballMagnet = intensity; });
      return true;
    }
    // "Set Guard Magnet": Damage Intensity (per hit point the live guards of his room have lost), Kill Intensity (per
    // guard killed so far), Distance Intensity (per pixel to the nearest live guard, x and y, his y smoothed through climbs)
    if (actionType == "Set Guard Magnet")
    {
      auto damage   = jaffarCommon::json::getNumber<float>(actionJs, "Damage Intensity");
      auto kill     = jaffarCommon::json::getNumber<float>(actionJs, "Kill Intensity");
      auto distance = jaffarCommon::json::getNumber<float>(actionJs, "Distance Intensity");
      // "Row Intensity" (optional): per pixel of height to the nearest live guard's row only (level 14's Jaffar: onto
      // his row, not onto him)
      auto row = actionJs.contains("Row Intensity") ? jaffarCommon::json::getNumber<float>(actionJs, "Row Intensity") : 0.0f;
      rule.addAction([=, this]() { _guardMagnet = guardMagnet_t{.damage = damage, .kill = kill, .distance = distance, .row = row}; });
      return true;
    }
    // "Set Room Heads Magnet": Room, Position (x), Row, Intensity: draws that room's live heads to that spot (their sum
    // of distances, see headsDistance), e.g. level 9's room 8 heads lured to its bottom left before he leaves it
    if (actionType == "Set Room Heads Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto room      = jaffarCommon::json::getNumber<uint8_t>(actionJs, "Room");
      auto x         = jaffarCommon::json::getNumber<int16_t>(actionJs, "Position");
      auto row       = jaffarCommon::json::getNumber<int8_t>(actionJs, "Row");
      rule.addAction([=, this]() { _headsMagnet = headsMagnet_t{.intensity = intensity, .room = room, .x = x, .row = row}; });
      return true;
    }
    // "Set Player HP Magnet": Intensity per hit point the prince has (e.g. the level 6 heads: room 7's two kill a prince
    // with one left)
    if (actionType == "Set Player HP Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _playerHPMagnet = intensity; });
      return true;
    }
    return false;
  }

  // Level 2's puzzle (kind1.c puzzle_check): each wrong sand tile pressed down after it rose (attribute bits 0..8 clear;
  // a sunk tile, which puzzle_check also counts as down, rises again later), the right one standing, the gate's count (bits 8..12 of room 1 position 10) and its opening (bits 0..4).
  // The right tile pressed after it rose (bits 0..8 clear) before the count reaches 10 stays down until room 1 is entered
  // again: -3. 0 until room 1
  // has been entered.
  __INLINE__ float puzzleProgress() const
  {
    if (_s->byte_14a0 == 0xFF) return 0.0f;
    const auto attr = [this](int tp) { return (uint16_t)*((const uint32_t*)((const uint8_t*)&_s->level + 0x348 + 1 * 0x78) + tp); };
    const uint16_t gate  = attr(10);
    if ((gate & 0x1F) != 0) return 6.0f + 5.0f + (float)(gate & 0x1F); // opening: the tiles no longer matter (the sunk ones must rise to be crossed)
    const int count = (gate >> 8) & 0x1F; // from 10 on the game presses the right tile itself
    float          p     = 0.0f;
    for (int i = 0; i < 6; i++)
    {
      const uint16_t a = attr(12 + i);
      if (i != _s->puzzle_answer) p += (a & 0x1FF) == 0 ? 1.0f : 0.0f; // pressed after it rose (a sunk tile rises again: nothing)
      else if ((a & 0xF) != 0 || count >= 10) p += 1.0f;
      else if ((a & 0x1F0) == 0) p -= 3.0f;
    }
    return p + (float)count / 4.0f + (float)(gate & 0x1F);
  }

  __INLINE__ jaffarCommon::hash::hash_t getStateInputHash() override { return {0, _s->Kid.frame}; }

  struct pointMagnet_t
  {
    float   intensity = 0.0;
    int16_t position  = 0;
  };

  quicker::QuickerSDLPoP2State* _s;

  std::vector<std::pair<size_t, size_t>> _hashTileModifiers;
  bool                                   _hashRandomSeed = false;
  bool                                   _hashClock      = false;
  bool                                   _hashRoomChars  = false;   // ("Hash Room Characters")
  bool                                   _pruneInputs    = false;
  bool                                   _restartAtCheckpoint = false;
  InputSet::inputIndex_t                 _restartInputIdx     = 0;

  std::vector<std::pair<pop2Input, InputSet::inputIndex_t>> _prunedInputs;

  pointMagnet_t _playerPosXMagnet;
  pointMagnet_t _playerPosYMagnet;
  float         _kidPosY = 0.0f;   // the prince's y smoothed through climbs (stateUpdatePostHook)
  uint8_t             _visited[roomCount + 1] = {};   // rooms visited so far (stateUpdatePostHook)
  uint8_t             _stood[roomCount + 1][3] = {};   // rows of rooms stood on so far (stateUpdatePostHook)
  uint8_t             _stoodHP[roomCount + 1][3] = {};   // his hit points when he first stood there
  uint8_t             _headsHunting[roomCount + 1] = {};   // per room, its heads alive and hunting (stateUpdatePostHook)
  std::vector<size_t> _hashVisited;                   // the rooms whose visited flag goes into the hash ("Hash Visited Rooms")
  std::vector<std::pair<size_t, size_t>> _hashStood;  // the "Stood On Row" flags that go into the hash ("Hash Stood On Rows")
  float         _playerDirectionMagnet = 0.0;
  float         _opponentHPMagnet      = 0.0;
  float         _playerHPMagnet        = 0.0;
  pointMagnet_t _opponentPosXMagnet;
  pointMagnet_t _opponentPosYMagnet;
  float         _puzzleMagnet          = 0.0;
  struct headsMagnet_t
  {
    float   intensity = 0.0;
    uint8_t room      = 0;
    int16_t x         = 0;
    int8_t  row       = 0;
  } _headsMagnet;
  float   _bigPotionMagnet  = 0.0;
  float   _bigPotionDist    = 0.0f;   // the prince to the nearest big potion of his room (stateUpdatePostHook)
  uint8_t _roomBigPotions   = 0;      // his room's potions and guards (stateUpdatePostHook; see the properties)
  uint8_t _roomOtherPotions = 0;
  uint8_t _roomLiveGuards   = 0;
  int8_t  _killBalance      = 0;
  uint8_t _roomGuardsDamage = 0;
  float   _guardDist        = 0.0f;   // the prince to the nearest live guard of his room (0: none)
  uint8_t _guardKills       = 0;      // (state: serialized)
  int8_t  _prevAlive[5]     = {};     // (state) each slot's guard on the last tick: -1 alive, 1 dead, 0 none
  uint8_t _prevRoom         = 0;      // (state) the prince's room on the last tick
  uint8_t _wastedKills      = 0;      // (state)
  uint8_t _prevDecided[5]   = {};     // (state) each slot's guard dead for good on the last tick
  struct guardMagnet_t
  {
    float damage   = 0.0;
    float kill     = 0.0;
    float distance = 0.0;
    float row      = 0.0;
  } _guardMagnet;
  float   _fireballMagnet   = 0.0;
  float   _fireballDist     = 0.0f;   // level 14's fireball to the nearest live guard (stateUpdatePostHook)
  float   _guardRowDist     = 0.0f;   // the prince's height to the nearest live guard's row
  uint8_t _roomGuardsOnRow  = 0;

  bool                 _levelEndLookahead = false;   // "Level End Lookahead"
  bool                 _musicOnAtStart    = false;   // "Music On At Start"
  std::vector<InputSet::inputIndex_t> _musicInputs;  // _prunedInputs' inputs with the Music On/Off key
  uint16_t             _levelEndWait      = 65535;   // (updateLevelEndWait)
  std::vector<uint8_t> _lookaheadBuffer;

  InputSet::inputIndex_t _nullInputIdx;
};

} // namespace sdlpop2

} // namespace games

} // namespace jaffarPlus
