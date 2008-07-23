#include <iostream>
#include <vector>
using namespace std;

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/IdentifierTable.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Sema/ParseAST.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"

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

    //cerr << Pos.getLineNumber() << ' ';
    cerr << Pos.getLogicalLineNumber() << ' ';
    cerr << Diags.getDescription(ID) << endl;
    for (int i = 0; i < NumStrs; ++i) {
      cerr << "\t%" << i << ": " << Strs[i] << endl;
    }
    cerr << endl;
  }
};


void addIncludePath(vector<DirectoryLookup>& paths,
    const string& path,
    DirectoryLookup::DirType type,
    FileManager& fm)
{
  // If the directory exists, add it.
  if (const DirectoryEntry *DE = fm.getDirectory(&path[0], 
                                                 &path[0]+path.size())) {
    bool isFramework = false;
    bool isUserSupplied = false;
    paths.push_back(DirectoryLookup(DE, type, isUserSupplied, isFramework));
    return;
  }
  cerr << "Cannot find directory " << path << endl;
}

class MyASTConsumer : public ASTConsumer {
public:
  virtual void HandleTopLevelDecl(Decl *D) {
    // XXX: need to check for all ScopedDecl subclasses, those imply that
    // D is not a variable...
    if (VarDecl *VD = dyn_cast<VarDecl>(D)
        && VD->isFileVarDecl()) {
      cerr << "Read top-level variable decl: '" << VD->getName() << "'\n";
    }
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


  // Add header search directories (C only, no C++ or ObjC)

  vector<DirectoryLookup> dirs;

  // user headers
  unsigned systemDirIdx = dirs.size();

  // system headers
  addIncludePath(dirs, "/usr/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/i686-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/powerpc-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);

  bool noCurDirSearch = false;  // search current directory, too
  headers.SetSearchPaths(dirs, systemDirIdx, noCurDirSearch);


  // Add input file

  const FileEntry* File = fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'" << endl;
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File, SourceLocation());
  //pp.EnterMainSourceFile();


  // Parse it

  ASTConsumer* c = new MyASTConsumer;
  ParseAST(pp, c);  // deletes c

  delete target;
}
