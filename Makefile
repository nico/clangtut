CXXFLAGS=-I/Users/nico/src/llvm-svn/tools/clang/include \
		 -fno-rtti \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --cxxflags`

# clangParse required starting from tut04
# clangAST and clangSema required starting from tut06
LDFLAGS=-lclangBasic -lclangLex -lclangParse -lclangSema -lclangAST \
		-lLLVMSupport -lLLVMSystem -lLLVMBitReader -lLLVMBitWriter \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --ldflags`

tut09: tut09_ast.o
	g++ $(LDFLAGS) -o tut09 tut09_ast.o

tut08: tut08_ast.o
	g++ $(LDFLAGS) -o tut08 tut08_ast.o

tut07: tut07_sema.o
	g++ $(LDFLAGS) -o tut07 tut07_sema.o

tut06: tut06_sema.o
	g++ $(LDFLAGS) -o tut06 tut06_sema.o

tut05: tut05_parse.o
	g++ $(LDFLAGS) -o tut05 tut05_parse.o

tut04: tut04_parse.o
	g++ $(LDFLAGS) -o tut04 tut04_parse.o

tut03: tut03_pp.o
	g++ $(LDFLAGS) -o tut03 tut03_pp.o

tut02: tut02_pp.o
	g++ $(LDFLAGS) -o tut02 tut02_pp.o

tut01: tut01_pp.o
	g++ $(LDFLAGS) -o tut01 tut01_pp.o

tut09_ast.o: tut09_ast.cpp
tut08_ast.o: tut08_ast.cpp
tut07_sema.o: tut07_sema.cpp
tut06_sema.o: tut06_sema.cpp
tut05_parse.o: tut05_parse.cpp
tut04_parse.o: tut04_parse.cpp
tut03_pp.o: tut03_pp.cpp
tut02_pp.o: tut02_pp.cpp
tut01_pp.o: tut01_pp.cpp

