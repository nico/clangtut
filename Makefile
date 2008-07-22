CXXFLAGS=-I/Users/nico/src/llvm-svn/tools/clang/include \
		 -fno-rtti \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --cxxflags`

LDFLAGS=-lclangBasic -lclangLex \
		-lLLVMSupport -lLLVMSystem -lLLVMBitReader -lLLVMBitWriter \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --ldflags`

tut02: tut02_pp.o
	g++ $(LDFLAGS) -o tut02 tut02_pp.o

tut01: tut01_pp.o
	g++ $(LDFLAGS) -o tut01 tut01_pp.o

tut02_pp.o: tut02_pp.cpp
tut01_pp.o: tut01_pp.cpp

