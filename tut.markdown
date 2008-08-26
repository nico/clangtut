How to parse C programs with clang
===

checkout and build llvm and clang.

See llvm-svn/tools/clang/docs/InternalsManual.html for a very rough overview.

* Preprocessor
* Lexer
* Parser
* Actions: Sema, diagnostics

clang does not have a stable API, so this tutorial might not be completely
up-to-date. The last time I checked that all programs work was
**Jul 22, 2008**.

Prereq
---

Checking out, building (graphviz support!), 

add /Applications/Graphviz.app/Contents/MacOS/ to path, config llvm

Tutorial 01: The bare minimum
---

Let's try to create `Preprocessor` object. Remember, this is how you
communicate with the lexer. Our first program will not do anything useful,
it only constructs a `Preprocessor` and exits again.

(XXX: it's possible to directly create a lexer as well; see `HTMLRewrite.cpp`.
This is for example useful in raw mode. Perhaps have a "Tut 0" that does
syntax highlighting only?)

The constructor takes no less than 5 arguments: A `Diagnostic` object, a
`LangOptions` object, a `TargetInfo` object, a `SourceManager` object, and
finally a `HeaderSearch` object. Let's break down what those objects are good
for, and how we can build them.

First is `Diagnostic`. This is XXX. To build a `Diagnostic` object, we need a
`DiagnosticClient`. `DummyDiagnosticClient`.

Next up is `LangOptions`. This is easy, as its constructor does not take any
parameters.

The `TargetInfo` is easy, too, but we need to call a factory method as the
constructor is private. Required so that the preprocessor can add
target-specific defines, for example `__APPLE__`.  Need to delete this at the
end of the program.

`SourceManager`.

Finally, `HeaderSearch` requires a `FileManager`. Good for XXX.

Need to compile with `-fno-rtti`, because clang is compiled that way, too. Else,

      Undefined symbols:
        "typeinfo for clang::DiagnosticClient", referenced from:
            typeinfo for DummyDiagnosticClientin tut01_pp.o
      ld: symbol(s) not found

when linking.

See `tut01_pp.cpp`

Tutorial 02: Processing a file
---

Right now, the program is not very interesting. It does not do anything yet.
We want to change the program so that it feeds C files into the preprocessor
object.

    pp.EnterSourceFile(sm.createFileID(File, SourceLocation()), 0);

Instead:

    sm.createMainFileID(File, SourceLocation());
    pp.EnterMainSourceFile();


Parse loop:

    Token Tok;
    do {
      pp.Lex(Tok);
      pp.DumpToken(Tok);
      cerr << endl;
    } while (Tok.isNot(tok::eof));

See `tut02_pp.cpp`. This is a very complete preprocessor, it handles all kinds
of corner cases correclty already. It also strips comments. Play around with
it a bit:

    XXX cat input01.c, show output

However, invalid input does not yet produce any error messages. That's because
our `DummyDiagnosticClient` does not yet produce any output. Let's change
this:

    XXX

XXX: incomplete. Now, we get error output.

With some effort, it is possible to turn this into a "real" preprocessor. See
`Driver/PrintPreprocessedOutput.cpp` in clang's source for how this could be
done.

See `tut02_pp.cpp`, `PPContext.h`, `input01.c`



Tutorial 03: Include files
---

If you feed something with an `#include` line into our program, chances are
that it does not work:

    XXX cat etc

(The "%0" output is because we don't fill in the arguments from the `Strs`
array).

We need to tell the preprocessor where it can find its header files. Where
standard include files are stored depends on your system. On Leopard, they are
in

    XXX

You add those paths via the `HeaderSearch` object. Its `SetSearchPaths()`
method takes a list of `DirectoryLookup`s (user headers at the front, system
headers after them), an index that specifies the first system header, and a
flag if the current directory should be searched for include files as well.

    XXX

Frameworks are special, they are looked into. Needs to be set e.g. for
`/Library/Frameworks`.

XXX: NormalHeaderDir vs SystemHeaderDir

See `tut03_pp.cpp`, `input03.c`. This only adds system paths for now.

Tutorial 04: Parsing the file
---

The preprocessor gives us a stream of tokens. We could now write a parser that
parses that token stream into an AST. Luckily, clang can do this for us.

`Action`, `MinimalAction`.

Change linker flags.

See `tut04_parse.cpp`, `input03.c`

Tutorial 05: Doing something interesting
---

`DeclTy` etc are abstract in parser. It calls method on the `Action` to create
them and then passes them again to the action object.

Actually surprisingly tricky. Eli Friedman:

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

`ActOnDeclarator()` (then mayhaps `AddInitializerToDecl`, then
`FinalizeDeclaratorGroup`. example with several decls in one line).

`DeclSpec` contains information about the declaration. `DeclaratorChunk`s
store modifiers like pointer, array, reference, or function. For example,

    int** (*bla[16]()[];

will have the following `DeclaratorChunk`s:

    XXX

`DeclSpec` itself stores storage specifiers, type, etc.

Need to change include dir type to ignore stuff from system headers.

Note that this does not find `static`s local to functions (which are globals,
too).

See [`tut05_parse.cpp`][tut05], [`input04.c`][in4]

[tut05]: tut05_parse.cpp
[in4]: input04.c

Tutorial 06: Doing semantic analysis with clang
---

Interface to sema is very minimalistic: Just a single function. Behind the
scenes, that function builds a `Parser` object and passes it an `Action` that
does the semantic analysis. The `Action` builds an AST while doing this.

We can now get rid of our `MyAction`. Instead, we do now need an
`ASTConsumer` (which will be deleted by sema).

BTW: last param prints stats. clang is obsessed with having a low memory
consumption :-)

`ParseAST()` also calls `EnterMainSourceFile()` on the preprocessor. So don't
call this yourself, else you get errors about duplicate definitions (because
the main source file is entered twice into the pp's file list).

Example:

    predefines>:3:15: error: redefinition of '__builtin_va_list'
    typedef char* __builtin_va_list;
                  ^
    <predefines>:3:15: error: previous definition is here
    typedef char* __builtin_va_list;

Happens also for stuff in your program.


Now with own `MyASTConsumer`. Code slightly simpler (and much more correct)
than the former version.

See [`tut06_sema.cpp`][tut06], [`input04.c`][in4]

[tut06]: tut06_sema.cpp

Tutorial 07: Support for real programs
---

-I, -D

http://llvm.org/docs/CommandLine.html

Nice output ?

Also take care of `static`s and print the name of the defining file.

`TranslationUnit` contains a whole AST (i.e. all toplevel decls)

See [`tut07_sema.cpp`][tut07], [`input05.c`][in5]

[tut07]: tut07_sema.cpp
[in5]: input05.c

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

See [`tut08_ast.cpp`][tut08], [`input06.c`][in6]

[tut08]: tut08_ast.cpp
[in6]: input06.c

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

Interlude: `DiagnosticClient`. In practice needs a `HeaderSearch` object to
implement `isInSystemHeader()`. However, many clients really only need
`hasErrorOccurred()` on `Diagnostics`, so making the HS mandatory for the
client (which is mandatory for D) causes grief for clients. But a
`TextDiagnosticClient` without HS is dangerous. Best fix: make HS mandatory
for DC, but DC not for D?

Furthermore, `HTMLDiagnositcs` shows warnings in system headers, TextDiag
doesn't.

Better: Fold TextDiagnostics into DiagnosticClient, base stuff on that. Make
HS mandatory for DC, etc. Problem: HS is in Lex, DC in Basic. Moving HS
requires moving DirectoryLookup.

Ted doesn't like this: Some DiagClients don't care about headers (if ast comes
from deserializing, e.g.). So isInSystemHeader() really shouldn't be in
DiagClient if it's only useful during lexing/pp/sema. Add method
`DiagFromFile` to `Diagnostics` and let it check for sys-header-dom? If this
should be disablable, add flag to `Diagnostics` (so DiagClient becomes purely
presentational).

Good: Clients do not have to take care of this

Need to ignore globals in system headers. Tricky: Where to put -I files? Need
them as users for some dirs (., proto), as system for others (python). For now
keep them as system and whitelist `.`. This drops proto, but that's ok for
now.

Careful: Need to collect globals on the rhs of toplevel inits, too (XXX).

What about multiple declarations (e.g. `gui`)?

See [`tut09_ast.cpp`][tut09], [`input07_1.c`][in7_1], [`input07_2.c`][in7_2],
and [`input07.h`][in7h].

[tut09]: tut09_ast.cpp
[in7h]: input07.h
[in7_1]: input07_1.c
[in7_2]: input07_2.c

ideas
---

Some words about codegen and analysis


Wrap up
---

clang is somewhat modular: you can plug in a new parser as long as it conforms to
the `Action` interface, several ast consumers can be used, etc.

codegen uses a visitor


questions
---

* Why does `-ast-dump` only print the first var in a decl?
* Why is `funcpointertype fp3` special?
* libDriver: `DefineBuiltinMacro`, `DeclPrinter`,
             `addIncludePath`, pp construction (inter alia header search),
             perhaps standard options (-D, -I, ?)

 <!--vim:set tw=78:-->
