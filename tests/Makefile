TARGET ?= dummy
SRCDIR := ..

LLVM_INSTALL_DIR := /usr/lib/llvm14
OPT := $(LLVM_INSTALL_DIR)/bin/opt
CLANG := $(LLVM_INSTALL_DIR)/bin/clang
CLANGPP := $(LLVM_INSTALL_DIR)/bin/clang++

OPTARGS := -enable-new-pm=0 -load $(SRCDIR)/build/LCM-PRE/libLCM.so -S

# Run like: "TARGET=dummy make run"
run: buildlib $(TARGET).ll
	make runorig
	make runpass
	make runpassgvn
	# make runreassoc

runorig:
	$(OPT) $(OPTARGS) ${TARGET}.ll -mem2reg -reassociate -mem2reg -simplifycfg -dot-cfg -o ${TARGET}.orig.ll
	$(CLANG) -lm -O0 -Wno-nullability-completeness ${TARGET}.orig.ll -o ${TARGET}.orig
	mv .main.dot ${TARGET}.main.orig.dot  # fixme: this only works for single funcs

runpass:
	$(OPT) $(OPTARGS)  ${TARGET}.ll -mem2reg -reassociate -lcm -mem2reg -simplifycfg -dot-cfg -o ${TARGET}.opt.ll
	$(CLANG) -lm -O0 -Wno-nullability-completeness ${TARGET}.opt.ll -o ${TARGET}.opt
	mv .main.dot ${TARGET}.main.opt.dot

runpassgvn:
	$(OPT) $(OPTARGS) ${TARGET}.ll -mem2reg -reassociate -gvn -mem2reg -simplifycfg -dot-cfg -o ${TARGET}.gvn.ll
	$(CLANG) -lm -O0 -Wno-nullability-completeness ${TARGET}.gvn.ll -o ${TARGET}.gvn
	mv .main.dot ${TARGET}.main.gvn.dot

runreassoc:
	$(OPT) $(OPTARGS) ${TARGET}.ll -reassociate -dot-cfg -o ${TARGET}.reassoc.ll
	$(CLANG) -lm -O0 -Wno-nullability-completeness ${TARGET}.reassoc.ll -o ${TARGET}.reassoc
	mv .main.dot ${TARGET}.main.reassoc.dot

runpassfull:
	$(OPT) $(OPTARGS) ${TARGET}.ll -reassociate -lcm -o ${TARGET}.optfull.ll
	mv .main.dot ${TARGET}.main.optfull.dot

opendot: run
	xdg-open ${TARGET}.main.orig.dot
	xdg-open ${TARGET}.main.opt.dot
	xdg-open ${TARGET}.main.reassoc.dot
	xdg-open ${TARGET}.main.optfull.dot

# run once at the beginning to configure cmake
config:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_C_COMPILER=$(CLANG) -DCMAKE_CXX_COMPILER=$(CLANGPP) -S$(SRCDIR) -B$(SRCDIR)/build -G "Unix Makefiles"

buildlib: $(SRCDIR)/LCM-PRE/LazyCodeMotion.cpp
	cmake --build $(SRCDIR)/build --config Debug --target LCM

%.ll: %.c
	$(CLANG) -Xclang -disable-O0-optnone -Wno-nullability-completeness -S -emit-llvm -c $< -o $@ 

clean:
	rm -f .*.dot *.dot *.ll *.orig *.opt *.gvn *.reassoc *.out *.log
	cd handwritten && rm -f *.dot *.ll *.orig *.opt *.gvn *.reassoc *.out *.log
	cd benchgame && rm -f *.dot *.ll *.orig *.opt *.gvn *.reassoc *.out *.log