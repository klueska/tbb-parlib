# Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
#
# This file is part of Threading Building Blocks.
#
# Threading Building Blocks is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# Threading Building Blocks is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Threading Building Blocks; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As a special exception, you may use this file as part of a free software
# library without restriction.  Specifically, if other files instantiate
# templates or use macros or inline functions from this file, or you compile
# this file and link it with other files to produce an executable, this
# file does not by itself cause the resulting executable to be covered by
# the GNU General Public License.  This exception does not however
# invalidate any other reasons why the executable file might be covered by
# the GNU General Public License.

#TBB_THREADING_LIB ?= pthread
TBB_THREADING_LIB ?= lithe
#TBB_THREADING_LIB ?= upthread
UPTHREAD_COMPAT_DIR ?= /usr/include/upthread/compatibility

TBB_THREADING_LIB := $(shell echo $(TBB_THREADING_LIB) | tr a-z A-Z)
define TBB_MACHINE_CONFIG
#define USE_$(TBB_THREADING_LIB) 1
#ifdef USE_UPTHREAD
	#define USE_PTHREAD 1
#endif
endef

COMPILE_ONLY = -c -MMD
PREPROC_ONLY = -E -x c++
INCLUDE_KEY = -I
DEFINE_KEY = -D
OUTPUT_KEY = -o #
OUTPUTOBJ_KEY = -o #
PIC_KEY = -fPIC
WARNING_AS_ERROR_KEY = -Werror
WARNING_KEY = -Wall
TEST_WARNING_KEY = -Wshadow -Wcast-qual -Woverloaded-virtual -Wnon-virtual-dtor $(if $(findstring cc4., $(runtime)),-Wextra)

WARNING_SUPPRESS = -Wno-parentheses -Wno-non-virtual-dtor
DYLIB_KEY = -shared
EXPORT_KEY = -Wl,--version-script,
LIBDL = -ldl

TBB_NOSTRICT = 1

CPLUS = g++
CONLY = gcc
LIB_LINK_FLAGS = $(DYLIB_KEY) -Wl,-soname=$(BUILDING_LIBRARY)
ifeq ($(TBB_THREADING_LIB),PTHREAD)
  LIBS += -lpthread
endif
ifeq ($(TBB_THREADING_LIB),LITHE)
  LIBS += -lithe
endif
ifeq ($(TBB_THREADING_LIB),UPTHREAD)
  LIBS += -lupthread
endif
LIBS += -lrt
LINK_FLAGS = -Wl,-rpath-link=.
C_FLAGS = $(CPLUS_FLAGS)
# gcc 4.4 and higher support -std=c++0x
ifneq (,$(shell gcc -dumpversion | egrep  "^(4\.[4-9]|[5-9])"))
    CPP11_FLAGS = -std=c++0x -D_TBB_CPP0X
endif

# gcc 4.2 and higher support OpenMP
ifneq (,$(shell gcc -dumpversion | egrep  "^(4\.[2-9]|[5-9])"))
    OPENMP_FLAG = -fopenmp
endif

ITT_NOTIFY = -DDO_ITT_NOTIFY
ifeq ($(cfg), release)
        CPLUS_FLAGS = $(ITT_NOTIFY) -g -O2 -DUSE_$(TBB_THREADING_LIB)
endif
ifeq ($(cfg), debug)
        CPLUS_FLAGS = -DTBB_USE_DEBUG $(ITT_NOTIFY) -g -O0 -DUSE_$(TBB_THREADING_LIB)
endif
ifeq ($(TBB_THREADING_LIB),UPTHREAD)
        CPLUS_FLAGS += -DUSE_PTHREAD -I$(UPTHREAD_COMPAT_DIR)
endif

ifneq (00,$(lambdas)$(cpp0x))
    CXX_ONLY_FLAGS += $(CPP11_FLAGS)
endif

TBB_ASM.OBJ=
MALLOC_ASM.OBJ=

ifeq (ia64,$(arch))
    ITT_NOTIFY =
# Position-independent code (PIC) is a must on IA-64, even for regular (not shared) executables
    CPLUS_FLAGS += $(PIC_KEY)
endif

ifeq (intel64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ia32,$(arch))
    CPLUS_FLAGS += -m32 -march=pentium4
    LIB_LINK_FLAGS += -m32
endif

ifeq (ppc64,$(arch))
    CPLUS_FLAGS += -m64
    LIB_LINK_FLAGS += -m64
endif

ifeq (ppc32,$(arch))
    CPLUS_FLAGS += -m32
    LIB_LINK_FLAGS += -m32
endif

ifeq (bgp,$(arch))
    CPLUS = mpicxx
    CONLY = mpicc
endif

# for some gcc versions on Solaris, -m64 may imply V9, but perhaps not everywhere (TODO: verify)
ifeq (sparc,$(arch))
    CPLUS_FLAGS    += -mcpu=v9 -m64
    LIB_LINK_FLAGS += -mcpu=v9 -m64
endif

ifneq ($(PARLIB_PREFIX),)
    CPLUS_FLAGS    += -I$(PARLIB_PREFIX)/include
    LIB_LINK_FLAGS += -L$(PARLIB_PREFIX)/lib
endif

ifneq ($(LITHE_PREFIX),)
    CPLUS_FLAGS    += -I$(LITHE_PREFIX)/include
    LIB_LINK_FLAGS += -L$(LITHE_PREFIX)/lib
endif

ifneq ($(UPTHREAD_PREFIX),)
    CPLUS_FLAGS    += -I$(UPTHREAD_PREFIX)/include
    LIB_LINK_FLAGS += -L$(UPTHREAD_PREFIX)/lib
endif

#------------------------------------------------------------------------------
# Setting assembler data.
#------------------------------------------------------------------------------
ASM = as
ifeq (intel64,$(arch))
    ASM_FLAGS += --64
endif
ifeq (ia32,$(arch))
    ASM_FLAGS += --32
endif
ifeq ($(cfg),debug)
    ASM_FLAGS += -g
endif

ASSEMBLY_SOURCE=$(arch)-gas
ifeq (ia64,$(arch))
    ASM_FLAGS += -xexplicit
    TBB_ASM.OBJ += atomic_support.o lock_byte.o log2.o pause.o ia64_misc.o
    MALLOC_ASM.OBJ += atomic_support.o lock_byte.o pause.o log2.o
endif
#------------------------------------------------------------------------------
# End of setting assembler data.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Setting tbbmalloc data.
#------------------------------------------------------------------------------

M_CPLUS_FLAGS = $(CPLUS_FLAGS) -fno-rtti -fno-exceptions -fno-schedule-insns2

#------------------------------------------------------------------------------
# End of setting tbbmalloc data.
#------------------------------------------------------------------------------
