/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_CODE_COMPRESSEDSTREAM_HPP
#define SHARE_CODE_COMPRESSEDSTREAM_HPP

#include "memory/allocation.hpp"
#include "utilities/unsigned5.hpp"
#include "utilities/moveBits.hpp"

// Simple interface for filing out and filing in basic types
// Used for writing out and reading in debugging information.

class CompressedStream : public ResourceObj {
  friend class VMStructs;
 protected:
  u_char* _buffer;
  int     _position;

 public:
  CompressedStream(u_char* buffer, int position = 0) {
    _buffer   = buffer;
    _position = position;
  }

  u_char* buffer() const               { return _buffer; }

  // Positioning
  int position() const                 { return _position; }
  void set_position(int position)      { _position = position; }
};


class CompressedReadStream : public CompressedStream {
 private:
  inline u_char read()                 { return _buffer[_position++]; }

 public:
  CompressedReadStream(u_char* buffer, int position = 0)
  : CompressedStream(buffer, position) {}

  jboolean read_bool()                 { return (jboolean) read();      }
  jbyte    read_byte()                 { return (jbyte   ) read();      }
  jchar    read_char()                 { return (jchar   ) read_int();  }
  jshort   read_short()                { return (jshort  ) read_signed_int(); }
  jint     read_signed_int();
  jfloat   read_float();               // jfloat_cast(reverse_bits(read_int()))
  jdouble  read_double();              // jdouble_cast(2*reverse_bits(read_int))
  jlong    read_long();                // jlong_from(2*read_signed_int())

  jint     read_int() {
    return UNSIGNED5::read_uint(_buffer, _position, 0);
  }
};

// Pack200 compression algorithm
class CompressedWriteStream : public CompressedStream {
 private:
  bool full() {
    return _position >= _size;
  }
  void store(u_char b) {
    _buffer[_position++] = b;
  }
  void write(u_char b) {
    if (full()) grow();
    store(b);
  }
  void grow();

 protected:
  int _size;

 public:
  CompressedWriteStream(int initial_size);
  CompressedWriteStream(u_char* buffer, int initial_size, int position = 0)
  : CompressedStream(buffer, position) { _size = initial_size; }

  void write_bool(jboolean value)      { write(value);      }
  void write_byte(jbyte value)         { write(value);      }
  void write_char(jchar value)         { write_int(value); }
  void write_short(jshort value)       { write_signed_int(value);  }
  void write_signed_int(jint value)    { write_int(UNSIGNED5::encode_sign(value)); }
  void write_float(jfloat value);      // write_int(reverse_bits(jint_cast(v)))
  void write_double(jdouble value);    // write_int(reverse_bits(<low,high>))
  void write_long(jlong value);        // write_signed_int(<low,high>)

  void write_int(juint value) {
    UNSIGNED5::write_uint_grow(value, _buffer, _position, _size,
                               [&](int){ grow(); });
  }
};

class CompressedSparseData : public ResourceObj {
protected:
  int     _position; // current byte offset
  size_t  _bit_position {0}; // current bit offset

public:
  CompressedSparseData(int position = 0) {
    _position = position;
  }
};

// Modified compression algorithm for a data set in which a significant part of the data is null
class CompressedSparseDataReadStream : public CompressedSparseData {
protected:
  const u_char* _buffer;
public:
  CompressedSparseDataReadStream(const u_char* buffer, int position) : CompressedSparseData(position) {
    _buffer = buffer;
  }

  const u_char* buffer() const { return _buffer; }

  void set_position(int pos) {
    _bit_position = 0;
    _position = pos;
  }

  jboolean read_bool()       { return read_int(); }
  jbyte    read_byte()       { return read_int(); }
  jint     read_signed_int() { return UNSIGNED5::decode_sign(read_int()); }
  jint     read_int();
  jdouble  read_double() {
    jint h = reverse_bits(read_int());
    jint l = reverse_bits(read_int());
    return jdouble_cast(jlong_from(h, l));
  }
  jlong    read_long() {
    jint low  = read_signed_int();
    jint high = read_signed_int();
    return jlong_from(high, low);
  }

protected:
  bool read_zero();
  uint8_t read_byte_impl();
  inline u_char read()       { return _buffer[_position++]; }
};

class CompressedSparseDataWriteStream : public CompressedSparseData {
protected:
  u_char* _buffer;
public:
  CompressedSparseDataWriteStream(int initial_size) : CompressedSparseData() {
    _buffer   = NEW_RESOURCE_ARRAY(u_char, initial_size);
    _size     = initial_size;
    _buffer[0] = 0;
  }

  const u_char* buffer() const { return _buffer; }

  void write_bool(jboolean value)   { write_int(value ? 1 : 0); }
  void write_byte(jbyte value)      { write_int(value); }
  void write_signed_int(jint value) { write_int(UNSIGNED5::encode_sign(value)); }
  void write_int(juint value);
  void write_double(jdouble value)  {
    juint rh = reverse_bits(high(jlong_cast(value)));
    juint rl = reverse_bits(low( jlong_cast(value)));
    write_int(rh);
    write_int(rl);
  }
  void write_long(jlong value)      {
    write_signed_int(low(value));
    write_signed_int(high(value));
  }

  void align() {
    if (_bit_position > 0) {
      // start a new byte
      _position++;
      _bit_position = 0;
    }
  }
  int position() {
    align(); // method positon() have a side effect: the current data becomes aligned
    return _position;
  }
  void set_position(int pos) {
    _position = pos;
    _bit_position = 0;
    assert(_position < _size, "set_position is only used for rollback");
  }

protected:
  int    _size;
  void grow();
  inline void grow_if_need() {
    if (_position >= _size) {
      grow();
    }
  }
  void write_zero();  // The zero word is encoded with a single zero bit
  void write_byte_impl(uint8_t b);
};

#endif // SHARE_CODE_COMPRESSEDSTREAM_HPP
