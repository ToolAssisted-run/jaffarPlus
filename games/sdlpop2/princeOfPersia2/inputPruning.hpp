#pragma once

// Input pruning for Prince of Persia 2: for a game state, the inputs grouped by what they lead to. Two inputs with the
// same key lead to the same game, so only one of each group needs trying. Transcribed from SDLPoP2's kidctl.c,
// control.c and kid.c (the prince's control step); checked against every input on 41k states of the quickerSDLPoP2 test
// movies (no two inputs with the same key ever led to different games).
//
// The input reaches the game only through the prince's control latches (kidctl.c read_user_control: -1 pressed and not
// yet used, 1 used and still held, 0 released) and the restart request of a dead prince (input.c). So the key is the
// branch control() takes and the latches it leaves. Where a branch depends on the tick's geometry (edges, gates, the
// opponent), the key keeps the inputs apart: it may keep more inputs than needed, never fewer.

#include <quickerSDLPoP2/quickerSDLPoP2.hpp>

namespace jaffarPlus
{

namespace games
{

namespace sdlpop2
{

// One input: x -1 left / 1 right, y -1 up / 1 down, shift 1 Shift / 2 Ctrl, keystroke a key pressed
struct pop2Input
{
  int8_t x, y, shift, keystroke;
};

class InputPruner
{
public:
  InputPruner(const quicker::QuickerSDLPoP2State* s) : _s(s) {}

  // The key of an input in the current state (inputs with equal keys lead to the same game)
  uint64_t key(const pop2Input& in)
  {
    const auto& kid = _s->Kid;
    _h              = 1469598103934665603ull;
    _memDead        = false;

    // input.c hotkeys_02be_core: a key or Shift/Ctrl while the prince is dead (and time is left) restarts the level
    const int  sh      = in.shift == 2 ? -2 : in.shift ? -1 : 0;
    const bool restart = (sh != 0 || in.keystroke) && ((_s->minutes_left != 0 && (int8_t)kid.alive > 6) || word_2ba8() != 0) && _s->word_5ce8 == 0 &&
                         !(_s->drawn_room == 4 && _s->level_number == 13 && shadow13Present());
    if (restart) return 1;

    // A dead prince: control() reads nothing, and a restart clears the latches (level.c level_begin)
    int alive = (int8_t)kid.alive;
    if (alive < 0 && kid.f12 == 0) alive = 0;
    if (alive >= 0) return 0;

    // kidctl.c: no directions when the level is over, outside the level or boarding the ship (seq 0x3B)
    int cx = in.x, cy = in.y;
    if ((int8_t)_s->word_32d8 != (int16_t)_s->counter_5cec || kid.room == 0 || kid.f19 == 0x3B) cx = cy = 0;

    // read_user_control, in screen terms ([0] left [1] right [2] up [3] down), then in the prince's facing terms
    const int8_t L = press(_s->kid_ctrl1_saved[0], cx == -1), R = press(_s->kid_ctrl1_saved[1], cx == 1);
    const int8_t U = press(_s->kid_ctrl1_saved[2], cy == -1), D = press(_s->kid_ctrl1_saved[3], cy == 1);
    int8_t       s = _s->kid_ctrl1_saved[4];
    if (sh == -1)
    {
      if (s != 1) s = -1;
    }
    else if (sh == -2)
    {
      if (s != 2) s = -2;
    }
    else
      s = 0;
    const bool dir0 = kid.direction == 0;
    latches    l    = {dir0 ? R : L, dir0 ? L : R, _s->word_5d38 ? D : U, _s->word_5d38 ? U : D, s};

    // control.c control(): the branch by action and frame
    const int frame = kid.frame, a = kid.action;
    bool      readsShiftAfter = a == 3 || a == 4; // kid.c try_grab_ledge, after the tick's move
    if (a == 5 || a == 4 || a == 9 || kid.f19 == 0x1B || kid.f19 == 0x6E)
    {
      add('R');
      rest(l);
    }
    else if (kid.f10 == 1)
      sword(l);
    else if (kid.charid >= 2)
    {
      add('Y');
      addDirs(l);
      add(l.s);
    }
    else if (frame == 15 || (frame >= 50 && frame <= 52))
      standing(l);
    else if (frame >= 45 && frame <= 49)
    {
      if (frame == 48 && l.s >= 0 && l.f != 0 && l.u == 0 && l.d == 0) add('q');
    }
    else if (frame >= 1 && frame <= 3)
    {
      if (l.u < 0 && l.f < 0)
      {
        rest(l);
        jump(l);
      }
    }
    else if (frame >= 67 && frame <= 69)
    {
      if (l.f != 0) jump(l);
    }
    else if (frame < 15)
      running(l, frame);
    else if (frame >= 87 && frame <= 99)
    {
      if (_s->level_kind != 1)
      {
        hanging(l, _s->word_8a84 > 0 ? _s->word_8a84 - 1 : 0);
        readsShiftAfter = true;
      } // (kid_input_and_control counts word_8a84 down first)
    }
    else if (frame == 81 && kid.f19 == 0x44)
    {
      add('e');
      rest(l);
      l.d             = 1;
      readsShiftAfter = true;
    }
    else if (frame == 109)
      crouched(l);
    else if ((frame >= 0xF6 && frame <= 0x105) || (frame >= 0x100 && frame <= 0x107))
    {
      add('Z');
      addDirs(l);
    } // control_with_sword: geometry first
    else if (frame == 44 || frame == 26)
      rest(l);

    // After the tick: the direction latches carry over (unless the next tick resets them before reading); a pending
    // Shift / Ctrl (-1 / -2) is read afresh next tick, only a used one (1 / 2) carries over
    if (!_memDead) addDirs(l);
    add((l.s == 1 || l.s == 2) ? l.s : 0);
    if (readsShiftAfter) add(100 + (l.s != 0));
    if (_s->level_kind == 1 && kid.room == 3) add(200 + l.s);    // kind1.c kind1_kid reads the latches
    if (_s->level_number == 5 && kid.room == 10) add(300 + l.s); // kid.c start_fall reads forward / backward
    return _h;
  }

private:
  struct latches
  {
    int8_t f, b, u, d, s; // forward, backward, up, down, shift
  };

  const quicker::QuickerSDLPoP2State* _s;
  uint64_t                            _h;
  bool                                _memDead; // the tick starts a sequence whose first frame resets the latches

  void          add(int64_t v) { _h = (_h ^ (uint64_t)(v + 0x9E3779B9)) * 0x100000001B3ull; }
  static int8_t press(int8_t c, bool held) { return c >= 0 ? (held ? (c == 0 ? -1 : c) : 0) : c; }
  static void   rest(latches& l) { l.f = l.b = l.u = l.d = 0; } // control_rest (Shift untouched)
  void          addDirs(const latches& l)
  {
    add(l.f);
    add(l.b);
    add(l.u);
    add(l.d);
  }

  uint16_t       word_2ba8() const { return *(const uint16_t*)(_s->tiles0 + 0xE); }
  uint16_t       word_4406() const { return *(const uint16_t*)((const uint8_t*)&_s->level + 0x184E); }
  const uint8_t* roomTiles(int room) const { return (const uint8_t*)&_s->level + (room - 1) * 30; }
  const uint8_t* roomLinks(int room) const { return (const uint8_t*)&_s->level + 0x17BC + room * 4; }
  bool           shadow13Present() const
  {
    const int8_t n = (int8_t)((const uint8_t*)&_s->level)[0x1867 + 3 * 0x74]; // ROOM_REC(4)->nchars
    for (int8_t i = 0; i < n && i < 5; i++)
      if (_s->chars[i].charid == 1) return true;
    return false;
  }

  // 1 when one of the tiles may be within 3 columns of the prince on his row (the column is recomputed from x before
  // control()); another room's edge or the level's edge counts as maybe
  bool tileNear(int t1, int t2, int t3) const
  {
    const auto& kid = _s->Kid;
    const int   row = kid.curr_row, col = kid.curr_col;
    if (row < 0 || row > 2 || kid.room == 0) return true;
    for (int c = col - 3; c <= col + 3; c++)
    {
      int room = kid.room, cc = c;
      if (cc < 0)
      {
        room = roomLinks(room)[0];
        cc += 10;
      }
      else if (cc > 9)
      {
        room = roomLinks(room)[1];
        cc -= 10;
      }
      if (room == 0 || cc < 0 || cc > 9) return true;
      const uint8_t t = roomTiles(room)[row * 10 + cc];
      if (t == t1 || t == t2 || t == t3) return true;
    }
    return false;
  }

  // control_standing_turn: backward = control_rest()
  void turn(latches& l)
  {
    add('T');
    rest(l);
    l.b = 1;
  }
  // control_jump_031062
  void jump(latches& l)
  {
    add('J');
    l.f = 1;
    l.u = 1;
  }
  // control_standing_up: the level door (the latches stay), a standing jump, or a jump up / grab
  void up(latches& l)
  {
    const bool door = _s->level.start_room != _s->drawn_room && _s->Kid.charid == 0 && tileNear(0x11, 0x11, 0x11);
    add('U');
    if (door) addDirs(l);
    if (l.f != 0)
      jump(l);
    else
    {
      rest(l);
      l.u = 1;
    }
  }
  // control_standing_down: down = 1, then a step off, a crouch or a climb down (geometry)
  void down(latches& l)
  {
    add('D');
    l.d = 1;
    addDirs(l);
  }
  // control_standing_step
  void step(latches& l)
  {
    add('S');
    l.f = 1;
    l.s = 1;
  }
  // control_standing_forward: a step (at an edge) or a run, then forward = control_rest()
  void forward(latches& l)
  {
    add('F');
    add(l.f < 0);
    add(l.s);
    const int8_t s = l.s;
    rest(l);
    l.f = 1;
    l.s = s;
  }

  // control_standing_branches (frames 15, 50..52)
  void standing(latches& l)
  {
    const auto& kid = _s->Kid;
    if (l.s == -1 && tileNear(0xA, 0x16, 0x16)) add('P'); // try_pick_up: only with a potion or the sword there
    if ((kid.f10 != 0xFF || (_s->level_kind == 6 && kid.charid == 1)) && l.s == -2 && l.f == 0 && l.b == 0 && l.u == 0 && l.d == 0)
    {
      add('W');
      rest(l);
      l.f = 1;
      l.s = 2;
      return; // control_standing_shift
    }
    // (the opponent close behind turns the prince: geometry; the key below keeps the inputs apart anyway)
    if (l.s == -1 || l.s == 1)
    {
      if (l.b < 0)
      {
        turn(l);
        return;
      }
      if (l.u < 0)
      {
        up(l);
        return;
      }
      if (l.d < 0)
      {
        down(l);
        return;
      }
      if (l.f < 0 && l.u == 0 && l.d == 0)
      {
        step(l);
        return;
      }
      if (l.f != 0)
      {
        add('N');
        return;
      }
    }
    if (l.f < 0)
    {
      if (l.u >= 0)
      {
        forward(l);
        return;
      }
    }
    else
    {
      if (l.b < 0)
      {
        turn(l);
        return;
      }
      if (l.u >= 0)
      {
        if (l.d < 0)
        {
          down(l);
          return;
        }
        if (l.f == 0)
        {
          add('N');
          return;
        }
        forward(l);
        return;
      }
      if (l.f >= 0)
      {
        up(l);
        return;
      }
    }
    jump(l);
  }

  // control_crouched (frame 109)
  void crouched(latches& l)
  {
    const auto& kid = _s->Kid;
    if (kid.f19 == 0x6F)
    {
      add('O');
      rest(l);
      l.s = 1;
      return;
    }
    add('C');
    if (tileNear(0xA, 0x16, 0x16)) add(l.s == -1);          // try_pick_up tests shift == -1
    if (_s->level_number == 5 && kid.room == 3) addDirs(l); // (an early return there keeps the latches)
    if (l.d == 0)
    {
      const bool gate = tileNear(4, 4, 4) || (word_4406() != 0 && (_s->level_kind == 2 || _s->level_kind == 4));
      if (!gate && !(_s->level_number == 5 && kid.room == 3))
      {
        add('s');
        _memDead = true;
        return;
      } // stand up (seq 0x31, action 5)
      add(0);
      addDirs(l);
      return; // under a gate: a hop or nothing, the latches stay
    }
    if (l.f >= 0)
    {
      rest(l);
      l.d = 1;
      add(1);
      return;
    }
    rest(l);
    l.f = 1;
    add(2);
  }

  // control_running (frames 0, 4..14)
  void running(latches& l, int frame)
  {
    if (l.f == 0 && l.b == 0 && (frame == 7 || frame == 11))
    {
      add('r');
      rest(l);
      l.f = 1;
    }
    else if (l.b != 0)
    {
      add('t');
      rest(l);
      l.b = 1;
    }
    else if (l.u != 0)
    {
      if (l.u < 0 && (frame == 7 || frame == 11))
      {
        add('j');
        rest(l);
        l.u = 1;
      }
    }
    else if (l.d < 0)
    {
      add('c');
      rest(l);
      l.d = 1;
    }
    if (l.f < 0) l.f = 1;
  }

  // control_hanging (frames 87..99)
  void hanging(latches& l, int w8a84)
  {
    if (w8a84 == 0 && l.u != 0)
    {
      add('h');
      rest(l);
      l.u = 1;
      l.s = 1;
      return;
    } // climb up
    if (l.s == 0)
    {
      add('l');
      rest(l);
      l.d = 1;
      return;
    } // let go
    add('g');
    addDirs(l); // held: geometry decides
  }

  // sword_actions for the prince (the frames decide the sequence ids)
  void swordActions(latches& l)
  {
    const int  fr    = _s->Kid.frame;
    const bool ready = fr == 0x9E || fr == 0xAA || fr == 0xAB;
    if (fr == 0xA1 && l.s >= 0)
    {
      add(1);
      return;
    }
    if (l.s == -2)
    {
      add(2);
      if (ready || fr == 0x9D || fr == 0xA5 || fr == 0x96 || fr == 0xA1)
      {
        l.s = 2;
        rest(l);
      }
      return;
    }
    if (l.d < 0)
    {
      add(3);
      if (ready)
      {
        rest(l);
        l.d = 1;
        add(30);
      }
      return;
    }
    if (l.u < 0)
    {
      add(4);
      if (ready || fr == 0xA8 || fr == 0xA5 || fr == 0xA7)
      {
        l.u = 1;
        add(40);
      }
      return;
    } // parry (the opponent decides the id)
    if (l.f < 0)
    {
      add(5);
      if (ready)
      {
        rest(l);
        l.f = 1;
      }
      return;
    }
    if (l.b < 0 && l.s == 0)
    {
      add(6);
      if (ready)
      {
        rest(l);
        l.b = 1;
      }
      return;
    }
    add(7);
  }

  // control_2fdf_1bfa for the prince (sword drawn): which of its paths runs depends on the tile under him and the
  // opponent, not on the input, so the key holds the outcome of every path
  void sword(latches& l)
  {
    const auto& kid = _s->Kid;
    const int   hd  = kid.hp_delta < 0 ? -kid.hp_delta : kid.hp_delta;
    if (kid.action > 1 || (int)kid.f12 <= hd)
    {
      add('x');
      addDirs(l);
      add(l.s);
      return;
    }
    const bool tailc = _s->level_number != 5 || kid.room != 10 || _s->word_927e < 1;
    latches    out   = l;
    for (int path = 0; path < 4; path++)
    {
      // 0: sword_actions then the tail; 1: the engage (seq 0x7F, action 5 first) then the tail; 2: the tail alone;
      // 3: the tail, then sheathe or sword_actions
      latches        t      = l;
      const uint64_t before = _h;
      _h                    = 0;
      bool dead             = path == 1;
      if (path == 0) swordActions(t);
      const bool engage = t.b < 0 && (t.s == -1 || t.s == 1) && tailc;
      if (engage)
      {
        add('E');
        t.s = 2;
        rest(t);
        t.b  = 1;
        dead = true;
      }
      else if (path == 3)
      {
        if (t.d < 0)
        {
          add('H');
          rest(t);
          t.d = 1;
        }
        else
          swordActions(t);
      }
      const uint64_t pathKey = _h;
      _h                     = before;
      add((int64_t)pathKey);
      if (!dead) addDirs(t);
      add((t.s == 1 || t.s == 2) ? t.s : 0);
      if (path == 0) out = t;
    }
    l        = out; // (the key already holds every path's latches)
    _memDead = true;
  }
};

} // namespace sdlpop2

} // namespace games

} // namespace jaffarPlus
