OPTARGS := -enable-new-pm=0 -load ../build/LCM-PRE/libLCM.so -lcm -S
TARGET ?= dummy.ll
SRCDIR := ..

# Run like: "TARGET=dummy.ll make run"
run: buildlib
	make $(TARGET)
	opt $(OPTARGS) ${TARGET}

# run once at the beginning to configure cmake
config:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_C_COMPILER=/usr/bin/clang-14 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-14 -S$(SRCDIR) -B$(SRCDIR)/build -G "Unix Makefiles"

buildlib: ../LCM-PRE/LazyCodeMotion.cpp
	cmake --build ../build --config Debug --target LCM

%.ll: %.c
	clang -S -emit-llvm -c $< -o $@ 
