CXXFLAGS=-I/Users/nico/src/llvm-svn/tools/clang/include \
		 -fno-rtti \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --cxxflags`

# clangParse required starting from tut04
LDFLAGS=-lclangBasic -lclangLex -lclangParse \
		-lLLVMSupport -lLLVMSystem -lLLVMBitReader -lLLVMBitWriter \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --ldflags`

tut04: tut04_parse.o
	g++ $(LDFLAGS) -o tut04 tut04_parse.o

tut03: tut03_pp.o
	g++ $(LDFLAGS) -o tut03 tut03_pp.o

tut02: tut02_pp.o
	g++ $(LDFLAGS) -o tut02 tut02_pp.o

tut01: tut01_pp.o
	g++ $(LDFLAGS) -o tut01 tut01_pp.o

tut04_parse.o: tut04_parse.cpp
tut03_pp.o: tut03_pp.cpp
tut02_pp.o: tut02_pp.cpp
tut01_pp.o: tut01_pp.cpp

