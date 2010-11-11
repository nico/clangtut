#include <iostream>
using namespace std;

#include "PPContext.h"
using namespace clang;


int main(int argc, char* argv[])
{
cout << LLVM_HOSTTRIPLE << endl;
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  PPContext context;

  // Add input file
  const FileEntry* File = context.fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File);
  context.pp->EnterMainSourceFile();

  // Parse it
  Token Tok;
  context.diagClient->BeginSourceFile(context.opts, context.pp.get());
  do {
    context.pp->Lex(Tok);
    if (context.diags.hasErrorOccurred())
      break;
    context.pp->DumpToken(Tok);
    cerr << endl;
  } while (Tok.isNot(tok::eof));
  context.diagClient->EndSourceFile();
}
