/* -*- mode: c++ -*-
 * Kaleidoscope-Chord -- Respond to chords of keys as a single keystroke
 * Copyright (C) 2023 Lisa Ugray
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kaleidoscope/plugin/Chord.h"

#include <Arduino.h>  // for PROGMEM
#include <stdint.h>   // for uint8_t, uint16_t, int8_t

#include "kaleidoscope/KeyAddr.h"               // for KeyAddr
#include "kaleidoscope/KeyEvent.h"              // for KeyEvent
#include "kaleidoscope/KeyEventTracker.h"       // for KeyEventTracker
#include "kaleidoscope/event_handler_result.h"  // for EventHandlerResult
#include "kaleidoscope/plugin.h"                // for Plugin
#include "kaleidoscope/key_defs.h"              // for Key, Key_Transparent
#include "kaleidoscope/progmem_helpers.h"       // for cloneFromProgmem
#include "kaleidoscope/keyswitch_state.h"       // for keyToggledOn
#include "kaleidoscope/Runtime.h"               // for Runtime

namespace kaleidoscope {
namespace plugin {
EventHandlerResult Chord::onKeyswitchEvent(KeyEvent &event) {
  if (event_tracker_.shouldIgnore(event)) {
    return EventHandlerResult::OK;
  }

  if (!keyToggledOn(event.state)) {
    resolveOrArpeggiate();
    return EventHandlerResult::OK;
  }

  if (potential_chord_size_ == 0) {
    if (!Runtime.hasTimeExpired(prior_keypress_timestamp_, minimum_prior_interval_)) {
      return EventHandlerResult::OK;
    }
    if (!isExpectedBeforeChord(event.key)) {
      prior_keypress_timestamp_ = Runtime.millisAtCycleStart();
    }
  }

  appendEvent(event);

  if (isChordStrictSubset()) {
    start_time_ = Runtime.millisAtCycleStart();
    return EventHandlerResult::ABORT;
  }

  Key target_key = getChord();

  if (target_key == Key_NoKey) {
    potential_chord_size_--;
    resolveOrArpeggiate();
    return EventHandlerResult::OK;
  }

  resetPriorKeypressTimestamp();
  potential_chord_size_ = 0;
  event.key             = target_key;
  return EventHandlerResult::OK;
}

EventHandlerResult Chord::afterEachCycle() {
  if (Runtime.hasTimeExpired(start_time_, timeout_) && potential_chord_size_ > 0) {
    resolveOrArpeggiate();
  }

  // If there hasn't been a keypress in a while, update the prior keypress
  // timestamp to avoid integer overflow issues:
  if (Runtime.hasTimeExpired(prior_keypress_timestamp_, minimum_prior_interval_)) {
    resetPriorKeypressTimestamp();
  }

  return EventHandlerResult::OK;
}

void Chord::setTimeout(uint8_t timeout) {
  timeout_ = timeout;
}

void Chord::setMinimumPriorInterval(uint8_t min_interval) {
  minimum_prior_interval_ = min_interval;
}

bool Chord::isExpectedBeforeChord(Key key) {
  return key.isKeyboardModifier()
    || key.isLayerShift()
    || key == Key_Space
    || key == Key_Esc
    || key == Key_Tab
    || key == Key_LeftArrow || key == Key_RightArrow || key == Key_UpArrow || key == Key_DownArrow;
}

void Chord::resetPriorKeypressTimestamp() {
  prior_keypress_timestamp_ = Runtime.millisAtCycleStart() - (minimum_prior_interval_ + 1);
}

void Chord::resolveOrArpeggiate() {
  Key target_key = getChord();
  if (target_key == Key_NoKey) {
    arpeggiate();
  } else {
    resolve(target_key);
  }
}

void Chord::resolve(Key target_key) {
  resetPriorKeypressTimestamp();

  KeyEvent event        = potential_chord_[potential_chord_size_ - 1];
  potential_chord_size_ = 0;

  KeyEventId stored_id    = event.id();
  KeyEvent restored_event = KeyEvent(event.addr, event.state, target_key, stored_id);
  Runtime.handleKeyEvent(restored_event);
}

bool Chord::inChord(uint8_t index, Key key) {
  for (uint8_t i = index; index < chord_defs_size_ - 1; i++) {
    Key chord_key = cloneFromProgmem(chord_defs_[i]);
    if (chord_key == Key_NoKey) {
      return false;
    }
    if (chord_key == key) {
      return true;
    }
  }
  return false;
}

bool Chord::isChordStrictSubset() {
  uint8_t c = 0;
  while (c < chord_defs_size_) {
    uint8_t cs = chordSize(c);
    if (isChordStrictSubsetOf(c, cs)) {
      return true;
    }
    c += cs + 2;
  }
  return false;
}

bool Chord::isChordStrictSubsetOf(uint8_t c, uint8_t cs) {
  if (cs <= potential_chord_size_) {
    return false;
  }
  for (uint8_t i = 0; i < potential_chord_size_; i++) {
    if (!inChord(c, potential_chord_[i].key)) {
      return false;
    }
  }
  return true;
}

uint8_t Chord::chordSize(uint8_t index) {
  uint8_t size = 0;
  uint8_t i    = index;
  while (i < chord_defs_size_ - 1) {
    Key key = cloneFromProgmem(chord_defs_[i]);
    if (key == Key_NoKey) {
      break;
    }
    size++;
    i++;
  }
  return size;
}

bool Chord::isChord(uint8_t c, uint8_t cs) {
  if (cs != potential_chord_size_) {
    return false;
  }
  for (uint8_t i = 0; i < potential_chord_size_; i++) {
    if (!inChord(c, potential_chord_[i].key)) {
      return false;
    }
  }
  return true;
}

Key Chord::getChord() {
  uint8_t c = 0;
  while (c < chord_defs_size_) {
    uint8_t cs = chordSize(c);
    if (isChord(c, cs)) {
      return cloneFromProgmem(chord_defs_[c + cs + 1]);
    }
    c += cs + 2;
  }
  return Key_NoKey;
}

void Chord::appendEvent(KeyEvent event) {
  if (potential_chord_size_ < kMaxChordSize) {
    potential_chord_[potential_chord_size_] = event;
    potential_chord_size_++;
  }
}

void Chord::arpeggiate() {
  for (uint8_t i = 0; i < potential_chord_size_; i++) {
    KeyEvent event          = potential_chord_[i];
    KeyEventId stored_id    = event.id();
    KeyEvent restored_event = KeyEvent(event.addr, event.state, event.key, stored_id);
    Runtime.handleKeyEvent(restored_event);
  }
  potential_chord_size_ = 0;
}
}  // namespace plugin
}  // namespace kaleidoscope

kaleidoscope::plugin::Chord Chord;
