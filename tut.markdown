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


 vim:set tw=78:
