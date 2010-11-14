#include <iostream>
using namespace std;

#include "PPContext.h"


int main(int argc, char* argv[])
{
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  PPContext context;

  // Add input file
  const clang::FileEntry* File = context.fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File);
  context.pp->EnterMainSourceFile();

  // Parse it
  clang::Token Tok;
  context.diagClient->BeginSourceFile(context.opts, context.pp.get());
  do {
    context.pp->Lex(Tok);
    if (context.diags.hasErrorOccurred())
      break;
    context.pp->DumpToken(Tok);
    cerr << endl;
  } while (Tok.isNot(clang::tok::eof));
  context.diagClient->EndSourceFile();
}
