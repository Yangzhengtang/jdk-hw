/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/oopStorage.hpp"
#include "jvmtifiles/jvmtiEnv.hpp"
#include "logging/log.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "oops/oop.inline.hpp"
#include "oops/weakHandle.inline.hpp"
#include "prims/jvmtiEventController.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiTagMapTable.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/macros.hpp"
#include "gc/shared/oopStorage.hpp"

JvmtiTagMapEntry::JvmtiTagMapEntry(oop obj): _obj(obj){}

JvmtiTagMapEntry::JvmtiTagMapEntry(const JvmtiTagMapEntry& src) {
   // move object into WeakHandle when copying into the table
   assert(src._obj != nullptr, "must be set");
   _wh = WeakHandle(JvmtiExport::weak_tag_storage(), src._obj);
   _obj = nullptr;
 }

JvmtiTagMapEntry::~JvmtiTagMapEntry(){
   // If obj is set null it out, this is called for stack object on lookup,
   // and it should not have a WeakHandle created for it yet.
   if (_obj != nullptr) {
     _obj = nullptr;
     assert(_wh.is_null(), "WeakHandle should be null");
   } else {
     _wh.release(JvmtiExport::weak_tag_storage());
  }
}

oop JvmtiTagMapEntry::object() const {
  assert(_obj == nullptr, "Must have a handle and not object");
  return _wh.resolve();
}

oop JvmtiTagMapEntry::object_no_keepalive() const {
  assert(_obj == nullptr, "Must have a handle and not object");
  return _wh.peek();
}

JvmtiTagMapTable::JvmtiTagMapTable()
  :_rrht_table(Constants::_table_size){
}

void JvmtiTagMapTable::clear() {
  struct RemoveAll{
    bool do_entry(JvmtiTagMapEntry   & entry, jlong const &  tag)
    {
      return true;
    }
  }RemoveAll;
  _rrht_table.unlink(&RemoveAll);

  assert(_rrht_table.number_of_entries() == 0, "should have removed all entries");
}

JvmtiTagMapTable::~JvmtiTagMapTable() {
  clear();
}

jlong JvmtiTagMapTable::find(oop obj) {
   //if (obj->fast_no_hash_check()) {
   //  return 0;
   //} else {
     JvmtiTagMapEntry jtme(obj);
     jlong* found = _rrht_table.get(jtme);
     return found == NULL ? 0 : *found;
   //}
 }

bool JvmtiTagMapTable::add(oop obj, jlong tag) {
  JvmtiTagMapEntry new_entry(obj);
  bool is_added = _rrht_table.put(new_entry, tag);
  assert(is_added, "should be added");
  return is_added;
}

bool JvmtiTagMapTable::remove(oop obj) {
  JvmtiTagMapEntry jtme(obj);
  return _rrht_table.remove(jtme);
}

void JvmtiTagMapTable::entry_iterate(JvmtiTagMapEntryClosure* closure) {
  _rrht_table.iterate(closure);
}

void JvmtiTagMapTable::resize_if_needed() {
  _rrht_table.maybe_grow();
}

void JvmtiTagMapTable::remove_dead_entries(GrowableArray<jlong>* objects) {
  struct IsDead{
    GrowableArray<jlong>* _objects;
    IsDead(GrowableArray<jlong>* objects) : _objects(objects){}
    bool do_entry(JvmtiTagMapEntry const & entry, jlong tag){
      if ( entry.object_no_keepalive() == NULL){
        if(_objects!=NULL){
          _objects->append(tag);
        }
        return true;
      }
      return false;;
    }
  }IsDead(objects);
  _rrht_table.unlink(&IsDead);
}


