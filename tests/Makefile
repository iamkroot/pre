OPTARGS := -enable-new-pm=0 -load ../build/lcmpre/libLLVMLCMPRE.so -lcmpre -S

buildlib: ../lcmpre/Pass.cpp
	cmake --build ../build --config Debug --target LLVMLCMPRE

# Run like: "TARGET=dummy.ll make run"
run: buildlib
	opt $(OPTARGS) ${TARGET}
