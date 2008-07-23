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

Tutorial 01: The bare minimum
---

Let's try to create `Preprocessor` object. Remember, this is how you
communicate with the lexer. Our first program will not do anything useful,
it only constructs a `Preprocessor` and exits again.

(XXX: it's possible to directly create a lexer as well; see `HTMLRewrite.cpp`.
This is for example useful in raw mode.)

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

See `tut03_pp.cpp`. This only adds system paths for now.

Tutorial 04: Parsing the file
---

The preprocessor gives us a stream of tokens. We could now write a parser that
parses that token stream into an AST. Luckily, clang can do this for us.

`Action`, `MinimalAction`.

Change linker flags.

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



Wrap up
---

clang is somewhat modular: you can plug in a new parser as long as it conforms to
the `Action` interface, several ast consumers can be used, etc.

 vim:set tw=78:
