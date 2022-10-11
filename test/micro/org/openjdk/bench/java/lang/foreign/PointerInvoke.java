/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package org.openjdk.bench.java.lang.foreign;

import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.TearDown;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

import java.lang.foreign.MemorySession;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.util.concurrent.TimeUnit;

@BenchmarkMode(Mode.AverageTime)
@Warmup(iterations = 5, time = 500, timeUnit = TimeUnit.MILLISECONDS)
@Measurement(iterations = 10, time = 500, timeUnit = TimeUnit.MILLISECONDS)
@State(org.openjdk.jmh.annotations.Scope.Thread)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Fork(value = 3, jvmArgsAppend = { "--enable-native-access=ALL-UNNAMED", "--enable-preview" })
public class PointerInvoke extends CLayouts {

    MemorySession session = MemorySession.openConfined();
    MemorySegment segment = session.allocate(100);

    static {
        System.loadLibrary("Ptr");
    }

    static final MethodHandle F_LONG, F_PTR;

    static {
        Linker abi = Linker.nativeLinker();
        SymbolLookup loaderLibs = SymbolLookup.loaderLookup();
        F_LONG = abi.downcallHandle(loaderLibs.find("func_as_long").get(),
                FunctionDescriptor.of(C_INT, C_LONG_LONG));
        F_PTR = abi.downcallHandle(loaderLibs.find("func_as_ptr").get(),
                FunctionDescriptor.of(C_INT, C_POINTER));
    }

    @TearDown
    public void tearDown() {
        session.close();
    }

    @Benchmark
    public int panama_call_as_long() throws Throwable {
        return (int)F_LONG.invokeExact(segment.address());
    }

    @Benchmark
    public int panama_call_as_address() throws Throwable {
        return (int)F_PTR.invokeExact(segment);
    }

    @Benchmark
    public int panama_call_as_new_segment() throws Throwable {
        MemorySegment newSegment = MemorySegment.ofAddress(segment.address(), 100, session);
        return (int)F_PTR.invokeExact(newSegment);
    }
}
