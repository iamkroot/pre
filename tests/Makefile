TARGET ?= dummy
SRCDIR := ..

LLVM_INSTALL_DIR := /usr/lib/llvm14
OPT := $(LLVM_INSTALL_DIR)/bin/opt
CLANG := $(LLVM_INSTALL_DIR)/bin/clang
CLANGPP := $(LLVM_INSTALL_DIR)/bin/clang++

OPTARGS := -mem2reg -opaque-pointers -enable-new-pm=0 -load $(SRCDIR)/build/LCM-PRE/libLCM.so

# Run like: "TARGET=dummy make run"
run: buildlib $(TARGET).ll
	make runorig
	mv .main.dot ${TARGET}.main.orig.dot  # fixme: this only works for single funcs
	make runpass
	mv .main.dot ${TARGET}.main.opt.dot

runorig:
	$(OPT) $(OPTARGS) ${TARGET}.ll -S -dot-cfg -o ${TARGET}.orig.ll

runpass:
	$(OPT) $(OPTARGS) ${TARGET}.ll -lcm -S -dot-cfg -o ${TARGET}.opt.ll

opendot: run
	xdg-open ${TARGET}.main.orig.dot
	xdg-open ${TARGET}.main.opt.dot

# run once at the beginning to configure cmake
config:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_C_COMPILER=$(CLANG) -DCMAKE_CXX_COMPILER=$(CLANGPP) -S$(SRCDIR) -B$(SRCDIR)/build -G "Unix Makefiles"

buildlib: $(SRCDIR)/LCM-PRE/LazyCodeMotion.cpp
	cmake --build $(SRCDIR)/build --config Debug --target LCM

%.ll: %.c
	$(CLANG) -Xclang -disable-O0-optnone -Wno-nullability-completeness -S -emit-llvm -c $< -o $@ 
