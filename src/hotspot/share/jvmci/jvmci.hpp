/*
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
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
 */

#ifndef SHARE_JVMCI_JVMCI_HPP
#define SHARE_JVMCI_JVMCI_HPP

#include "compiler/compiler_globals.hpp"
#include "compiler/compilerDefinitions.hpp"
#include "utilities/exceptions.hpp"

class BoolObjectClosure;
class constantPoolHandle;
class JavaThread;
class JVMCIEnv;
class JVMCIRuntime;
class Metadata;
class MetadataHandleBlock;
class OopClosure;
class OopStorage;

template <size_t>
class FormatStringEventLog;

typedef FormatStringEventLog<256> StringEventLog;

struct _jmetadata;
typedef struct _jmetadata *jmetadata;

class JVMCI : public AllStatic {
  friend class JVMCIRuntime;
  friend class JVMCIEnv;

 private:
  // List of libjvmci based JVMCIRuntimes.
  // Should only be accessed under JVMCI_lock.
  static JVMCIRuntime* _compiler_runtimes;

  // Special libjvmci based JVMCIRuntime reserved for
  // threads trying to attach when in JVMCI shutdown.
  // This preserves the invariant that JVMCIRuntime::for_thread()
  // never returns nullptr.
  static JVMCIRuntime* _shutdown_compiler_runtime;

  // True when at least one JVMCIRuntime::initialize_HotSpotJVMCIRuntime()
  // execution has completed successfully.
  static volatile bool _is_initialized;

  // True once boxing cache classes are guaranteed to be initialized.
  static bool _box_caches_initialized;

  // Handle created when loading the JVMCI shared library with os::dll_load.
  // Must hold JVMCI_lock when initializing.
  static void* _shared_library_handle;

  // Argument to os::dll_load when loading JVMCI shared library
  static char* _shared_library_path;

  // Records whether JVMCI::shutdown has been called.
  static volatile bool _in_shutdown;

  // Access to the HotSpot heap based JVMCIRuntime
  static JVMCIRuntime* _java_runtime;

  // The file descriptor to which fatal_log() writes. Initialized on
  // first call to fatal_log().
  static volatile int _fatal_log_fd;

  // The path of the file underlying _fatal_log_fd if it is a normal file.
  static const char* _fatal_log_filename;

  // Native thread id of thread that will initialize _fatal_log_fd.
  static volatile intx _fatal_log_init_thread;

  // JVMCI event log (shows up in hs_err crash logs).
  static StringEventLog* _events;
  static StringEventLog* _verbose_events;
  enum {
    max_EventLog_level = 4
  };

  // Gets the Thread* value for the current thread or NULL if it's not available.
  static Thread* current_thread_or_null();

 public:

  enum CodeInstallResult {
     ok,
     dependencies_failed,
     cache_full,
     nmethod_reclaimed,
     code_too_large,
     first_permanent_bailout = code_too_large
  };

  // Returns true iff JVMCIThreadsPerNativeLibraryRuntime == 0.
  static bool using_singleton_shared_library_runtime() {
    return JVMCIThreadsPerNativeLibraryRuntime == 0;
  }

  // Returns true iff there is a new shared library JavaVM per compilation.
  static bool one_shared_library_javavm_per_compilation() {
    return JVMCIThreadsPerNativeLibraryRuntime == 1 && JVMCICompilerIdleDelay == 0;
  }

  // Gets the handle to the loaded JVMCI shared library, loading it
  // first if not yet loaded and `load` is true. The path from
  // which the library is loaded is returned in `path`.
  static void* get_shared_library(char*& path, bool load);

  // Logs the fatal crash data in `buf` to the appropriate stream.
  static void fatal_log(const char* buf, size_t count);

  // Gets the name of the opened JVMCI shared library crash data file or NULL
  // if this file has not been created.
  static const char* fatal_log_filename() { return _fatal_log_filename; }

  static void do_unloading(bool unloading_occurred);

  static void metadata_do(void f(Metadata*));

  static void shutdown(JavaThread* thread);

  // Returns whether JVMCI::shutdown has been called.
  static bool in_shutdown();

  static bool is_compiler_initialized();

  /**
   * Determines if the VM is sufficiently booted to initialize JVMCI.
   */
  static bool can_initialize_JVMCI();

  static void initialize_globals();

  static void initialize_compiler(TRAPS);

  // Ensures the boxing cache classes (e.g., java.lang.Integer.IntegerCache) are initialized.
  static void ensure_box_caches_initialized(TRAPS);

  // Increments a value indicating some JVMCI compilation activity
  // happened on `thread` if it is a CompilerThread.
  // Returns `thread`.
  static JavaThread* compilation_tick(JavaThread* thread);

  // Gets the single runtime for JVMCI on the Java heap. This is the only
  // JVMCI runtime available when !UseJVMCINativeLibrary.
  static JVMCIRuntime* java_runtime()     { return _java_runtime; }

  // Gets the JVMCI shared library runtime associated with `thread`.
  // This must only be called when UseJVMCINativeLibrary is true.
  // If `create` is true and there is no runtime currently associated with
  // `thread`, this method creates one.
  static JVMCIRuntime* compiler_runtime(JavaThread* thread, bool create=true);

  // Appends an event to the JVMCI event log if JVMCIEventLogLevel >= `level`
  ATTRIBUTE_PRINTF(2, 0) static void vlog(int level, const char* format, va_list ap);

  // Traces an event to tty if JVMCITraceLevel >= `level`
  ATTRIBUTE_PRINTF(2, 0) static void vtrace(int level, const char* format, va_list ap);

 public:
  // Log/trace a JVMCI event
  ATTRIBUTE_PRINTF(2, 3) static void event(int level, const char* format, ...);
  ATTRIBUTE_PRINTF(1, 2) static void event1(const char* format, ...);
  ATTRIBUTE_PRINTF(1, 2) static void event2(const char* format, ...);
  ATTRIBUTE_PRINTF(1, 2) static void event3(const char* format, ...);
  ATTRIBUTE_PRINTF(1, 2) static void event4(const char* format, ...);
};

// JVMCI event macros.
#define JVMCI_event_1 if (JVMCITraceLevel < 1 && JVMCIEventLogLevel < 1) ; else ::JVMCI::event1
#define JVMCI_event_2 if (JVMCITraceLevel < 2 && JVMCIEventLogLevel < 2) ; else ::JVMCI::event2
#define JVMCI_event_3 if (JVMCITraceLevel < 3 && JVMCIEventLogLevel < 3) ; else ::JVMCI::event3
#define JVMCI_event_4 if (JVMCITraceLevel < 4 && JVMCIEventLogLevel < 4) ; else ::JVMCI::event4

#endif // SHARE_JVMCI_JVMCI_HPP
