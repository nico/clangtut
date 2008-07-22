#include <iostream>
using namespace std;

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/FileManager.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang;

class DummyDiagnosticClient : public DiagnosticClient {
public:
  virtual void HandleDiagnostic(Diagnostic &Diags, 
                                Diagnostic::Level DiagLevel,
                                FullSourceLoc Pos,
                                diag::kind ID,
                                const std::string *Strs,
                                unsigned NumStrs,
                                const SourceRange *Ranges, 
                                unsigned NumRanges) {

    cerr << endl << endl;

    switch (DiagLevel) {
    default: assert(0 && "Unknown diagnostic type!");
    case Diagnostic::Note:    cerr << "note: "; break;
    case Diagnostic::Warning: cerr << "warning: "; break;
    case Diagnostic::Error:   cerr << "error: "; break;
    case Diagnostic::Fatal:   cerr << "fatal error: "; break;
      break;
    }

    cerr << Diags.getDescription(ID) << endl << endl;
  }
};


int main(int argc, char* argv[])
{
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }


  // Create Preprocessor object

  DummyDiagnosticClient diagClient;
  Diagnostic diags(diagClient);

  LangOptions opts;

  TargetInfo* target = TargetInfo::CreateTargetInfo("i386-apple-darwin");

  SourceManager sm;

  FileManager fm;
  HeaderSearch headers(fm);

  Preprocessor pp(diags, opts, *target, sm, headers);


  // Add input file

  const FileEntry* File = fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File, SourceLocation());
  pp.EnterMainSourceFile();


  // Parse it

  Token Tok;
  do {
    pp.Lex(Tok);
    pp.DumpToken(Tok);
    cerr << endl;
  } while (Tok.isNot(tok::eof));

  delete target;
}
