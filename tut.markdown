How to parse C programs with clang: A tutorial in 9 parts
===



<p class="by">written by <a href="mailto:nicolasweber@gmx.de">Nicolas
Weber</a></p>


Introduction
---

From clang's [website][clang]:

> The goal of the Clang project is to create a new C, C++, Objective C and
> Objective C++ front-end for the [LLVM][llvm] compiler. 

What does that mean, and why should you care? A front-end is a  program that
takes a piece of code and converts it from a flat string to a structured,
tree-formed representation of the same program -- an AST.

Once you have an AST of a program, you can do many things with the program
that are hard without an AST. For example, renaming a variable without an AST
is hard: You cannot simply search for the old name and replace it with a new
name, as this will change too much (for example, variables that have the old
name but are in a different namespace, and so on). With an AST, it's easy: In
principle, you only have to change the `name` field of the right
`VariableDeclaration` AST node, and convert the AST back to a string. (In
practice, it's a bit harder for [some codebases][cscout-paper]).

Front-ends have existed for decades. So, what's special about clang? I think
the most interesting part is that clang uses a library design, which means
that youc an easily embed clang into your own programs (and by "easily", I
mean it. Most programs in this tutorial are well below 50 lines). This
tutorial tells you how you do this.

So, do you have a large C code-base and want to create [automated non-trivial
refactorings][gtk-refactor]? Would you like to have a [ctags][] that works
better with C++ and at all with Objective-C? Would you like to collect some
statistics about your program, and you feel that `grep` doesn't cut it? Then
clang is for you.

This tutorial will offer a tour through clang's preprocessor, parser, and AST
libraries. As example, we will write a program that collects a list of all
globals defined and used in a program. For [these](input07.h)
[input](input07_1.c) [files](input07_2.c), this [output](input07.html) will be
generated. The final program is close to production quality: It is for example
able to process the source code of [vim](http://vim.org) -- the resulting html
file is 2.4 mb! (vim uses lots of globals.) Furthermore, the program can be
used with any make-based program out of the box -- no change to Makefiles or
input file lists required.

A short word of warning: clang is still work in prograss. C++-support is not
yet anywhere near completion, and clang does not have a stable API, so this
tutorial might not be completely up-to-date. The last time I checked that all
programs work was **Aug 26, 2008**.

Clang works on all platforms. In this tutorial I assume that you have some
Unix-based platform, but in principle everything should work on Windows, too.

[clang]: http://clang.llvm.org
[llvm]: http://llvm.org
[cscout-paper]: http://www.spinellis.gr/pubs/jrnl/2003-TSE-Refactor/html/Spi03r.html
[gtk-refactor]: http://people.imendio.com/richard/gtk-rewriter/
[ctags]: http://ctags.sourceforge.net/


Getting started
---

There is no official release of clang yet, you have to get it from SVN and
build it for yourself. Luckily, this is easy:

    svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm
    cd llvm
    PATH=$PATH:/Applications/Graphviz.app/Contents/MacOS ./configure
    make -j2

    cd tools
    svn co http://llvm.org/svn/llvm-project/cfe/trunk clang
    cd clang
    make -j2

If you call `./configure` without [graphviz][] in your `PATH`, that's fine
too. You'll be missing some visualization functionality, but we won't use that
in this tutorial anyway.

Note that this does a debug build of llvm and clang. The resulting libraries
and binaries will end up in `llvm/Debug` and are a lot slower than the release
binaries (which you get when running `./configure --enable-optimized`).
    
You can find more information in clang's [official getting started
guide](http://clang.llvm.org/get_started.html). By the way, if you're using
Time Machine, you probably want to emit the llvm folder from your backup --
else, 700mb get backed up every time you update and recompile llvm and clang.

XXX: [Browse clang source](https://llvm.org/svn/llvm-project/cfe/trunk/).

<!--
XXX

* Preprocessor
* Lexer
* Parser
* Actions: Sema, diagnostics

(XXX: it's possible to directly create a lexer as well; see `HTMLRewrite.cpp`.
This is for example useful in raw mode. Perhaps have a "Tut 0" that does
syntax highlighting only?)
-->

[graphviz]: http://pixelglow.com/graphviz/

Tutorial 01: The bare minimum
---

A front-end consists of multiple parts. First is usually a lexer, which
converts the input from a stream of characters to a stream of tokens. For
example, the input `while` is converted from the five characters 'w', 'h',
'i', 'l', and 'e' to the token `kw_while`. For performance reasons, clang does
not have a separate preprocessor program, but does preprocessing while lexing.

The `Preprocessor` class is the main interface to the lexer, and it's a class
you will need in almost every program that embeds clang. So, for starters, 
let's try to create `Preprocessor` object. Our first program will not do
anything useful, it only constructs a `Preprocessor` and exits again.

The constructor of `Preprocessor` takes no less than 5 arguments: A
`Diagnostic` object, a `LangOptions` object, a `TargetInfo` object, a
`SourceManager` object, and finally a `HeaderSearch` object. Let's break down
what those objects are good for, and how we can build them.

First is `Diagnostic`. This is used by clang to report errors and warnings to
the user. A `Diagnostic` object can have a `DiagnosticClient`, which is
responsible for actually displaying the messages to the user. We will use
clang's built-in `TextDiagnostics` class, which writes errors and warnings to
the console (it's the same `DiagnosticClient` that is used by the `clang`
binary).

Next up is `LangOptions`. This class lets you configure if you're compiling C
or C++, and which language extensions you want to allow. Constructing this
object is easy, as its constructor does not take any parameters.

The `TargetInfo` is easy, too, but we need to call a factory method as the
constructor is private. The factory method takes a "host triple" as parameter
that defines the architecture clang should compile for, such as
"i386-apple-darwin". We will pass `LLVM_HOST_TRIPLE`, which contains the host
triple describing the machine llvm was compiled on.  But in principle, you can
use clang as a [cross-compiler][] very easily, too.  The `TargetInfo` object
is required so that the preprocessor can add target-specific defines, for
example `__APPLE__`. You need to delete this object at the end of the program.

`SourceManager` is used by clang to load and cache source files. Again, its
constructor takes no arguments.

Finally, the constructor of `HeaderSearch` requires a `FileManager`.
`HeaderSearch` configures where clang looks for include files.

So, to build a `Preprocessor` object, the following code is required:

    TextDiagnostics diagClient;
    Diagnostic diags(&diagClient);
    LangOptions opts;
    TargetInfo* target = TargetInfo::CreateTargetInfo(LLVM_HOST_TRIPLE);
    SourceManager sm;
    FileManager fm;
    HeaderSearch headers(fm);
    Preprocessor pp(diags, opts, *target, sm, headers);
    // ...
    delete target;

Because we will need a `Preprocessor` object in all tutorials, I've moved this
initialization code to its own file, `PPContext.h`. With this file,
`tut01_pp.cpp` is quite short (take your time and take a look at that two
files).

So, all that is left is to compile `tut01_pp.cpp` and you're done with part 1!
This is how you do it:

    g++ -I/Users/$USER/src/llvm/tools/clang/include \
      `/Users/$USER/src/llvm/Debug/bin/llvm-config --cxxflags` \
      -fno-rtti -c tut01_pp.cpp
    g++ `/Users/$USER/src/llvm/Debug/bin/llvm-config --ldflags` \
      -lLLVMSupport -lLLVMSystem -lLLVMBitReader -lLLVMBitWriter \
      -lclangBasic -lclangLex -lclangDriver \
      -o tut01 tut01_pp.o

You need to compile with `-fno-rtti`, because clang is compiled that way, too.
Else, the linker will complain with this when linking:

      Undefined symbols:
        "typeinfo for clang::DiagnosticClient", referenced from:
            typeinfo for DummyDiagnosticClientin tut01_pp.o
      ld: symbol(s) not found

Also note that you need to link quite a lot of libraries. Both clang and LLVM
have fine-grained libraries, so that you can link in just what you need.

So, that's it for part 1! Now you can run your first shiny, clang-powered
program:

    ./tut01

Sure enough, it doesn't do anything yet. Let's tackle this next.

[cross-compiler]: http://en.wikipedia.org/wiki/Cross_compile


Tutorial 02: Processing a file
---

Right now, out program is not very interesting, as it does not do anything yet.
We want to change it so that it feeds C files into the preprocessor object. To
add an input file to the preprocessor, we call:

    const FileEntry* file = fm.getFile("filename.c");
    sm.createMainFileID(file, SourceLocation());
    pp.EnterMainSourceFile();

This creates a file id for the input file and stores it as the "main file" in
the `SourceManager` object. The second line tells the preprocessor to enter
the `SourceManager`'s main file into its file list; the call does some other
setup stuff (such as creating a buffer with predefines), too.

Now that a source file is added, we can ask the preprocessor to preprocess it
and read the preprocessed input tokens:

    Token Tok;
    do {
      pp.Lex(Tok);  // read one token
      if (context.diags.hasErrorOccurred())  // stop lexing/pp on error
        break;
      pp.DumpToken(Tok);  // outputs to cerr
      cerr << endl;
    } while (Tok.isNot(tok::eof));

See `tut02_pp.cpp` for the complete program. Note that this is a very complete
preprocessor, it handles all kinds of corner cases correctly already. It also
strips comments. The program can be compiled exactly like `tut01`.

After building, play around with it a bit. For example, here's its output when
`input01.c` is given as an input file:

    $ ./tut02 input01.c 
    typedef 'typedef'
    char 'char'
    star '*'
    identifier '__builtin_va_list'
    semi ';'
    int 'int'
    identifier 'main'
    l_paren '('
    r_paren ')'
    l_brace '{'
    int 'int'
    identifier 'a'
    equal '='
    numeric_constant '4'
    plus '+'
    numeric_constant '5'
    semi ';'
    int 'int'
    identifier 'b'
    equal '='
    input01.c:5:13: warning: "/*" within block comment
      /* Nested /* comments */ 12; // are handled correctly
                ^
    numeric_constant '12'
    semi ';'
    int 'int'
    identifier 'c'
    equal '='
    numeric_constant '12'
    semi ';'
    r_brace '}'
    eof ''

Note the `typedef char* __builtin_va_list;` that gets added through the
predefine buffer.  Also note the nicely formatted warning message
`TextDiagnostics` gives us for free.  You can also try feeding errorneous
programs to `tut02`. As you'll see, some errors are detected, while others are
not (yet). For example, try `input02.c`.


Tutorial 03: Include files
---

If you feed something with an `#include` line into our program, chances are
that it does not work. For example, if we feed it `input03.c`, we get an
error:

    $ ./tut02 input03.c 
    typedef 'typedef'
    char 'char'
    star '*'
    identifier '__builtin_va_list'
    semi ';'
    input03.c:1:10: error: 'stdio.h' file not found
    #include <stdio.h>
             ^

We need to tell the preprocessor where it can find its header files. Where
standard include files are stored depends on your system. On Leopard, they are
for example in `/usr/lib/gcc/i686-apple-darwin/4.0.1/include`.

You add those paths via the `HeaderSearch` object. Its `SetSearchPaths()`
method takes a list of `DirectoryLookup`s (user headers at the front, system
headers after them), an index that specifies the first system header, and a
flag if the current directory should be searched for include files as well.

However, clang does luckily include a helper class that makes setting up
search paths much easier. This helper class is called `InitHeaderSearch`. To
add the default system header paths to clang, you use the following code:

    InitHeaderSearch init(headers);
    init.AddDefaultSystemIncludePaths(opts);
    init.Realize();  // this actually sends the header list to HeaderSearch

With this tiny change, `#include` directives that include system headers do
already work. See `tut03_pp.cpp` for the complete program.

    $ ./tut03 input03.c
    # ... lots of output omitted
    semi ';'
    r_brace '}'
    int 'int'
    identifier 'main'
    l_paren '('
    r_paren ')'
    l_brace '{'
    identifier 'printf'
    l_paren '('
    string_literal '"Hello, world\n"'
    r_paren ')'
    semi ';'
    r_brace '}'
    eof ''

This actually outputs all the tokens found in the included file, so the output
is quite long.

With some effort, it is possible to turn this into a "real" preprocessor. See
`Driver/PrintPreprocessedOutput.cpp` in clang's source for how this could be
done. (XXX: link clang sources to their declarations in clang's websvn)


Tutorial 04: Parsing the file
---

The preprocessor gives us a stream of tokens. We could now write a parser that
parses that token stream into an AST. Luckily, clang can do this for us.
However, clang decouples the parser from AST construction, meaning that you
can use the parser, but not use clang's AST.

So, how does clang's parser work? The constructor of the `Parser` class takes
an object of type `Action`. `Action` is an interface class that has a virtual
method for everything that is parsed. For example, `ActOnStartOfFunctionDef()`
is called when the parser parses the start of a function definition. At the
time of this writing, there are close to 100 `ActOn*()` methods which you can
override.

Nearly all overrides are optional. However, `isTypeName()` must be specified
so that the parser can differentiate XXX.

Luckily, the class `MinimalAction` already implements this bit of required
minimal semantic checking for us, so we can simply use this. Here's how the
parser is used:

    IdentifierTable tab(context.opts);
    MinimalAction action(tab);
    Parser p(context.pp, action);
    p.ParseTranslationUnit();

That's actually all we're going to do in this part. To compile the program,
you need to add `-lclangParse` to the linker flags. See `tut04_parse.cpp` for
the complete program. To make the program slightly less boring, it outputs
some statistics about the program it parses (example input is `input03.c`):

    $ ./tut04 input03.c

    *** Identifier Table Stats:
    # Identifiers:   83
    # Empty Buckets: 8109
    Hash density (#identifiers per bucket): 0.010132
    Ave identifier length: 8.084337
    Max identifier length: 28

    Number of memory regions: 1
    Bytes allocated: 1901


Tutorial 05: Doing something interesting
---

By now, we can do some actual analysis of the input code. Remember, we want to
write a program that lists information about global variables. Finding all
globals in a program is a good start. One possible approach is to use the
parser for this.

Every time the parser finds a declarator, the parser calls `ActOnDeclarator()`
on the action object. Thus, we override this method in our own subclass of
`MinimalAction`.

What is a declarator? It is a C statement that declares something -- a
variable, a function, a struct, a typedef. Here are some examples of
declarators:

    const unsigned long int b;  // b is just a constant
    int *i;  // declares (and defines) a pointer-to-int variable
    extern int a;  //declares an int variable
    void printf(const char* c, ...);  // a function prototype
    typedef unsigned int u32;  // a typedef
    struct { int a } t;  // a variable that has an anonymous struct as type
    char *(*(**foo [][8])())[];  // ...wtf? see below :-)

A declaration has very roughly two parts: the declared type on the left (e.g.
`const unsigned long int` or `struct { int a }`) and a list of "modifiers" and
the variable name itself on the right (e.g. `b`, `*i`, or `*(*(**foo
[][8])())[]`). The part on the left is represented by a `DeclSpec` in clang,
the (potentially complex) part on the right is a list of `DeclaratorChunk`s. A
`DeclaratorChunk` has a `Kind` that can be one of `Pointer`, `Reference`,
`Array`, or `Function`.

As explained [here][c-decl] (go read it!), the last declarator above has this
type:

> foo is array of array of 8 pointer to pointer to function returning pointer
> to array of pointer to char

Accordingly, the `DeclaratorChunk` passed to our callback has a list of
`DeclaratorChunk`s that contain, in that order, the following `Kind`s:
`Array`, `Array`, `Pointer`, `Pointer`, `Pointer`, `Function`, `Pointer`,
`Array`, `Pointer`. Each of the chunks contains more information (e.g. the
array size for `Array` chunks).

Since we want to detect global variables, we need to skip `Declarator`s whose
first `DeclaratorChunk` has `Kind` `Function`. Luckily, `DeclaratorChunk` has
the method `isFunctionDeclarator()` which does this check, so we can use that.

Furthermore, we're only interested in declarations at file scope (as opposed
to, say, function parameters or local declarations). To check this,
`Declarator` offers the `getContext()` function, which returns `FileContext`
for declarators at file scope.

We are not interested in declarators that start with `typedef` or `extern`
(we're looking for definition of globals, not their declaration). As `typedef`
and `extern` belong the the "left" part of a declarator, information about
them is stored in the declarator's `DeclSpec`.

Finally, we are not interested in globals from system headers. To check for
this, we need to know which file the declarator was found in. clang uses the
class `SourceLocation` to represent this information. Since there are lots of
`SourceLocation`s (every identifier needs to store where it is from, for
example), some effort has been done to make `SourceLocation` small -- it's
only 32 bit, as small as an `int`. As a result, a `SourceLocation` alone
cannot tell you much, you also need a `SourceManager`, which works together
with `SourceLocation` for full-featured file identification. `SourceManager`
has a method `isInSystemHeader(loc)` that we can use. (XXX: difference between
file ids and macro ids.)

All in all, our `ActOnDeclarator()` method looks like this:

    virtual Action::DeclTy *
    ActOnDeclarator(Scope *S, Declarator &D, DeclTy *LastInGroup) {
      const DeclSpec& DS = D.getDeclSpec();
      SourceLocation loc = D.getIdentifierLoc();

      if (D.getContext() == Declarator::FileContext
          && DS.getStorageClassSpec() != DeclSpec::SCS_extern
          && DS.getStorageClassSpec() != DeclSpec::SCS_typedef
          && !D.isFunctionDeclarator()
          && !pp.getSourceManager().isInSystemHeader(loc)) {
        IdentifierInfo *II = D.getIdentifier();
        cerr << "Found global declarator " << II->getName() << endl;
      }
      return MinimalAction::ActOnDeclarator(S, D, LastInGroup);
    }

`DeclTy` is just a typedef for `void`. Every `Action` can decide how it wants
to represent declarations. We use whatever `MinimalAction` uses. The only
parameter that is interesting to us is `Declarator &D`. 

The complete program is `tut05_parse.cpp`. Here's its output for `input04.c`:

    Found global user declarator a
    Found global user declarator a
    Found global user declarator b
    Found global user declarator c
    Found global user declarator funcp
    Found global user declarator fp2
    Found global user declarator fp3
    Found global user declarator f
    Found global user declarator f2
    Found global user declarator t

The program works well, but it has a few quirks. Once, it believes that `f`
and `f2` are globals, when they are really just function declarations. With
the words of [Eli Friedman][eli], from clang's excellent and very helpful
[mailing list][clang-list]:

> Fundamentally, in C, it's impossible to
> tell apart a global function declaration and a global variable
> declaration without resolving typedefs and __typeof expressions.
> Since that's beyond the scope of the parser, you'll either need to use
> Sema, do these yourself, or be a bit conservative about printing out
> things that aren't actually globals.
> 
> Examples of function declarations without an explicit type declarator:
> 
>     typedef int x();
>     x z;
>     __typeof(z) r;

We will fix this problem in the next part.

Also note that this does not find `static`s local to functions (which are
globals, too). We won't fix this in the tutorial, but it's not too hard to do
if you want to give it a shot.

[c-decl]: http://eli.thegreenplace.net/2008/07/18/reading-c-type-declarations/
[eli]: http://article.gmane.org/gmane.comp.compilers.clang.devel/2103
[clang-list]: http://lists.cs.uiuc.edu/mailman/listinfo/cfe-dev


Tutorial 06: Doing semantic analysis with clang
---

To differentiate variable declarations from function prototypes, we need to do
semantic analysis. Again, clang can do this for us. Its Sema library is
basically just an `Action` object that is passed to the parser. However, all
this happens behind the scenes.

The interface to Sema is very minimalistic: It's just a single function,
`ParseAST()`. This function takes a `Preprocessor` object and an
`ASTConsumer`. Behind the scenes, that function builds a `Parser` object and
passes it an `Action` that does the semantic analysis. This `Action` builds an
AST while doing this.

We can now get rid of our `MyAction`. Instead, we do now need an
`ASTConsumer`, which you can think of as an `Action` for Sema. The method we
want to override is `HandleTopLevelDecl()`, which is called (surprise!) for
every top-level declaration. The method gets passed a `Decl` object, which is
the AST class used for declarations. `HandleTopLevelDecl()` has a minor quirk:
if a declaration contains multiple variables (e.g. `int a, b;`), it is called
only once, and the `Decl` that's passed in is a linked list that you need to
walk (this will be changed [to something more sane][sane-decl] in the future,
though).

We want to handle only `VarDecl`s, and only those that are global and not
`extern`. This yields

    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      for (; VD; VD = cast_or_null<VarDecl>(VD->getNextDeclarator())) {
        if (VD->isFileVarDecl() && VD->getStorageClass() != VarDecl::Extern)
          cerr << "Read top-level variable decl: '" << VD->getName() << "'\n";
      }
    }

Note that this is much simpler than what we did in the last part -- and it's
more correct too. Yay.

Our new program is shown in `tut06_sema.cpp`.  `ParseAST()` also calls
`EnterMainSourceFile()` on the preprocessor. So don't call this yourself, else
you get errors about duplicate definitions (because the main source file is
entered twice into the pp's file list), such as

    predefines>:3:15: error: redefinition of '__builtin_va_list'
    typedef char* __builtin_va_list;
                  ^
    <predefines>:3:15: error: previous definition is here
    typedef char* __builtin_va_list;

Here's what our program prints for `input04.c`:

    Read top-level variable decl: 'a'
    Read top-level variable decl: 'a'
    Read top-level variable decl: 'b'
    Read top-level variable decl: 'c'
    Read top-level variable decl: 'funcp'
    Read top-level variable decl: 'fp2'
    Read top-level variable decl: 'fp3'
    Read top-level variable decl: 't'

Observe that `f` and `f2` are now correctly omitted.

Our program is now already good enough to handle real-life code (and we're
still well below 50 lines), but it's interface is not. Let's tackle this next.

[sane-decl]: http://lists.cs.uiuc.edu/pipermail/cfe-dev/2008-August/002589.html


Tutorial 07: Support for real programs
---

Compiling "real" source files usually requires that several flags are passed
to the compiler. Flags that are commonly used are `-I` to add directories to
the include file search path, and `-D` to define macros at the command line.

If we make our program compatible to the flags that are used by common C
compilers, then we can analyze existing programs simply by changing the
compiler. Hence, we'll add support for `-I` and `-D` to our program.

Furthermore, "real" compilation invocations ususally pass several other flags
that are specific to code generation. Those flags are not important to our
program. Hence, our program will accept arbitrary flags but will ignore them.

Adding support for command-line parameters usually requires a fair amount of
code. Luckily, we don't have to write this code: clang belongs to the LLVM
project, and LLVM has a very powerful command-line parameter handling library.
It also has [great documentation][llvm-commandline], hence I won't explain it
in detail. In short:

XXX

Code necessary to implement `-I` (easy) and `-D` (currently harder than it
should be). Slurp rest, need special-case slurper for `-o`.

Vim:

    gcc -c -I. -Iproto -DHAVE_CONFIG_H -DFEAT_GUI_MAC -fno-common \
     -fpascal-strings -Wall -Wno-unknown-pragmas -mdynamic-no-pic \
     -pipe -I. -Iproto -DMACOS_X_UNIX -no-cpp-precomp \
     -I/Developer/Headers/FlatCarbon  -g -O2 \
     -I/System/Library/Frameworks/Python.framework/Versions/2.5/include/python2.5 \
     version.c -o objects/version.o

thus:

    static llvm::cl::list<std::string>
    D_macros("D", llvm::cl::value_desc("macro"), llvm::cl::Prefix,
        llvm::cl::desc("Predefine the specified macro"));

    static llvm::cl::list<std::string>
    I_dirs("I", llvm::cl::value_desc("directory"), llvm::cl::Prefix,
        llvm::cl::desc("Add directory to include search path"));

    static llvm::cl::opt<string>
    InputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"),
        llvm::cl::Required);

    static llvm::cl::list<std::string> IgnoredParams(llvm::cl::Sink);
    static llvm::cl::opt<string> dummy("o", llvm::cl::desc("dummy for gcc compat"));

Nice output ? -- html output

Also take care of `static`s and print the name of the defining file.

`TranslationUnit` contains a whole AST (i.e. all toplevel decls)

See `tut07_sema.cpp`, `input05.c`

[llvm-commandline]: http://llvm.org/docs/CommandLine.html


Tutorial 08: Working with the AST
---

`StmtVisitor`, strangely recurring pattern

traversion: iterators, visitors

visitor only visits a single element (?) -- need to recurse manually.
Careful: the child iterator can return NULL (e.g. `if` without else branch
will have NULL for the else branch).

expr derived from stmt

Find uses of all globals (in one file only?)

Example in `StmtDumper`.

XXX: Test with functions containing macros.

only prints uses in functions, that omits some uses (e.g. as initializers of
other globals)

LogicalLineNumber works with macro expansion, LineNumber does not.

See `tut08_ast.cpp`, `input06.c`


Tutorial 09: Tracking globals across files
---

To identify globals over several translation units: Combination
filename/global name should be unique (global name alone is unique for
non-static vars). If static vars in functions should be handled, function name
becomes important, too.

Do it like a compiler: For each file, output an .o file, and build the
complete file during "link time". The .o file could be

* A copy of the input file
* A serialized AST of the input file
* A serialized form of the extracted data

Problem with 1.: Need to remember compile flags, paths, etc

Problem with 3.: Likely need manual (de)serializing code, not really flexible

Second options seems to hit the sweet spot: Shows some more clang, keeps
parsing paralellizable, but keeps .o files general.

Sadly, serializing is still incomplete, so we can't do that. So, go with 3 for
now. We need to serialize for each file:

* (List of declared globals)
* List of defined globals (with information if they are static to that file)
* List of global uses, perhaps along with a few lines of context

The linker then can for each global list all uses.

Need to ignore globals in system headers. Tricky: Where to put -I files? Need
them as users for some dirs (., proto), as system for others (python). For now
keep them as system and whitelist `.`. This drops proto, but that's ok for
now.

Careful: Need to collect globals on the rhs of toplevel inits, too (XXX).

What about multiple declarations (e.g. `gui`)?

See `tut09_ast.cpp`, `input07_1.c`, `input07_2.c`, and `input07.h`.


ideas
---

Some words about codegen and analysis


Wrap up
---

clang is somewhat modular: you can plug in a new parser as long as it conforms to
the `Action` interface, several ast consumers can be used, etc.

codegen uses a visitor

analysis lib is cool and wasn't covered at all.


questions
---

* libDriver: `DefineBuiltinMacro`, `DeclPrinter`,
             pp construction (inter alia header search),
             perhaps standard options (-D, -I, ?)


Download
---

You can download this tutorial along with all sources, the Makefile, etc as
a [zip file](clangtut.zip).


TODO
---

XXX: script that can parse `EXEC: cmd` lines, then use this for program
output, code excerpts, etc

XXX: use markdown-python with highlight-code extensions (which uses pygments)


 <!--vim:set tw=78 ft=markdown:-->
