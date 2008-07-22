How to parse C programs with clang
===

checkout and build llvm and clang.

See llvm-svn/tools/clang/docs/InternalsManual.html for a very rough overview.

* Preprocessor
* Lexer
* Parser
* Actions: Sema, diagnostics

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
constructor is private. (XXX: What's this good for in the PP?) Need to delete
this at the end of the program.

`SourceManager`.

Finally, `HeaderSearch` requires a `FileManager`. Good for XXX.

Need to compile with `-fno-rtti`, because clang is compiled that way, too. Else,

      Undefined symbols:
        "typeinfo for clang::DiagnosticClient", referenced from:
            typeinfo for DummyDiagnosticClientin tut01_pp.o
      ld: symbol(s) not found

when linking.

See `tut01_pp.cpp`


 vim:set tw=78:
