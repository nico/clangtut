CXXFLAGS=-I/Users/nico/src/llvm-svn/tools/clang/include \
		 -fno-rtti \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --cxxflags`

# clangParse required starting from tut04
# clangAST and clangSema required starting from tut06
# clangRewrite required in tut09
LDFLAGS=-lclangBasic -lclangLex -lclangParse -lclangSema -lclangAST \
		-lLLVMSupport -lLLVMSystem -lLLVMBitReader -lLLVMBitWriter \
		-lclangRewrite -lclangDriver \
		 `/Users/nico/src/llvm-svn/Debug/bin/llvm-config --ldflags`

all: tut01 tut02 tut03 tut04 tut05 tut06 tut07 tut08 tut09

clean:
	rm -rf tut01 tut02 tut03 tut04 tut05 tut06 tut08 tut09
	rm -rf tut01_pp.o tut02_pp.o tut03_pp.o tut04_parse.o tut05_parse.o
	rm -rf tut06_sema.o tut07_sema.o tut08_ast.o tut09_ast.o
	rm -rf input07_1.o input07_2.o input07.html

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

test:
	make runtest 2>&1 | cmp witness.txt -

runtest: all
	./tut01
	./tut02 input01.c
	./tut03 input03.c
	./tut04 input03.c
	./tut05 input04.c
	./tut06 input04.c
	./tut07 -DTEST input05.c
	./tut08 -DTEST input06.c
	./tut09 -o input07_1.o input07_1.c
	./tut09 -o input07_2.o input07_2.c
	./tut09 -o input07.html input07_1.o input07_2.o
	cat input07.html

# XXX: SmartyPants also converts "" in program code; it shouldn't
html: tut.markdown
	echo '<!DOCTYPE html>' > tut.html
	echo '<html><head>' >> tut.html
	echo '<meta charset="UTF-8"><title>clang tutorial</title>' >> tut.html
	echo '<link rel="stylesheet" href="tut.css" type="text/css">' >> tut.html
	#echo '<style type="text/css" src="tut.css"></style>' >> tut.html
	echo '</head><body><div class="page">' >> tut.html
	python linkify.py tut.markdown | Markdown.pl | SmartyPants.pl >> tut.html
	echo '</div></body></html>' >> tut.html
	open -a Safari tut.html

upload: test html
	rm -rf clangtut
	mkdir clangtut
	cp tut*.cpp PPContext.h input*.c input*.h input07.html tut.html tut.css Makefile linkify.py clangtut

tut09_ast.o: tut09_ast.cpp PPContext.h
tut08_ast.o: tut08_ast.cpp PPContext.h
tut07_sema.o: tut07_sema.cpp PPContext.h
tut06_sema.o: tut06_sema.cpp PPContext.h
tut05_parse.o: tut05_parse.cpp PPContext.h
tut04_parse.o: tut04_parse.cpp PPContext.h
tut03_pp.o: tut03_pp.cpp PPContext.h
tut02_pp.o: tut02_pp.cpp PPContext.h
tut01_pp.o: tut01_pp.cpp

