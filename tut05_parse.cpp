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

#include "clang/Parse/DeclSpec.h"
#include "clang/Parse/Parser.h"

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

    cerr << Diags.getDescription(ID) << endl;
    for (int i = 0; i < NumStrs; ++i) {
      cerr << "\t%" << i << ": " << Strs[i] << endl;
    }
    cerr << endl;
  }
};


void addIncludePath(vector<DirectoryLookup>& paths,
    const string& path,
    FileManager& fm)
{
  // If the directory exists, add it.
  if (const DirectoryEntry *DE = fm.getDirectory(&path[0], 
                                                 &path[0]+path.size())) {
    bool isFramework = false;
    bool isUserSupplied = false;
    paths.push_back(DirectoryLookup(DE, DirectoryLookup::NormalHeaderDir,
          isUserSupplied, isFramework));
    return;
  }
  cerr << "Cannot find directory " << path << endl;
}

class MyAction : public MinimalAction {
public:
  MyAction(IdentifierTable& tab) : MinimalAction(tab) {}

  //virtual StmtResult ActOnDeclStmt(DeclTy *Decl, SourceLocation StartLoc,
                                   //SourceLocation EndLoc) {
    //cerr << "Found declstmt " << Decl << endl;
    //return MinimalAction::ActOnDeclStmt(Decl, StartLoc, EndLoc);
  //}

  Action::DeclTy *
  ActOnDeclarator(Scope *S, Declarator &D, DeclTy *LastInGroup) {
    // Print names of global variables. Differentiating between
    // global variables and global functions is Hard in C, so this
    // is only an approximation.
    if (D.getContext() == Declarator::FileContext) {
      IdentifierInfo *II = D.getIdentifier();
      const DeclSpec& DS = D.getDeclSpec();

      if (DS.getStorageClassSpec() != DeclSpec::SCS_extern
          && DS.getStorageClassSpec() != DeclSpec::SCS_typedef) {


        if (D.getNumTypeObjects() == 0 ||
            D.getTypeObject(D.getNumTypeObjects() - 1).Kind
              != DeclaratorChunk::Function) {
        cerr << "Found global declarator " << II->getName() << endl;
        }
        
        //for (int i = 0; i < D.getNumTypeObjects(); ++i) {
          //DeclaratorChunk dc = D.getTypeObject(i);
          //cerr << dc.Kind << " ";
        //}
        //cerr << endl;
      }
    }

    return MinimalAction::ActOnDeclarator(S, D, LastInGroup);
  }
  //virtual void AddInitializerToDecl(DeclTy *Dcl, ExprTy *Init) {
    //cerr << "Initializer found " << endl;
  //}
  //virtual DeclTy *FinalizeDeclaratorGroup(Scope *S, DeclTy *Group) {
    ////cerr << "finalize declarator" << endl;
    //return MinimalAction::FinalizeDeclaratorGroup(S, Group);
  //}
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
  addIncludePath(dirs, "/usr/include", fm);
  addIncludePath(dirs, "/usr/lib/gcc/i686-apple-darwin9/4.0.1/include", fm);
  addIncludePath(dirs, "/usr/lib/gcc/powerpc-apple-darwin9/4.0.1/include", fm);

  bool noCurDirSearch = false;  // search current directory, too
  headers.SetSearchPaths(dirs, systemDirIdx, noCurDirSearch);


  // Add input file

  const FileEntry* File = fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'" << endl;
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File, SourceLocation());
  pp.EnterMainSourceFile();


  // Parse it

  IdentifierTable tab(opts);
  MyAction action(tab);
  Parser p(pp, action);
  p.ParseTranslationUnit();


  delete target;
}
