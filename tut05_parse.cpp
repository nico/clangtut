#include <iostream>
#include <vector>
using namespace std;

#include "clang/Basic/IdentifierTable.h"
#include "clang/Parse/DeclSpec.h"
#include "clang/Parse/Parser.h"
#include "clang/Driver/InitHeaderSearch.h"
#include "PPContext.h"
using namespace clang;

class MyAction : public MinimalAction {
  const Preprocessor& pp;
public:
  MyAction(IdentifierTable& tab, const Preprocessor& prep)
    : MinimalAction(tab), pp(prep) {}

  Action::DeclTy *
  ActOnDeclarator(Scope *S, Declarator &D, DeclTy *LastInGroup) {
    // Print names of global variables. Differentiating between
    // global variables and global functions is Hard in C, so this
    // is only an approximation.

    // Only global declarations...
    if (D.getContext() == Declarator::FileContext) {
      IdentifierInfo *II = D.getIdentifier();
      const DeclSpec& DS = D.getDeclSpec();

      // ...that aren't typedefs or `extern` declarations...
      if (DS.getStorageClassSpec() != DeclSpec::SCS_extern
          && DS.getStorageClassSpec() != DeclSpec::SCS_typedef) {

        // ...and no functions...
        if (D.getNumTypeObjects() == 0 ||
            D.getTypeObject(D.getNumTypeObjects() - 1).Kind
              != DeclaratorChunk::Function) {

          SourceLocation loc = DS.getTypeSpecTypeLoc();
          // ...and in a user header
          SourceManager& sm = pp.getSourceManager();
          HeaderSearch& headers = pp.getHeaderSearchInfo();
          const FileEntry *DeclFile = sm.getFileEntryForLoc(loc);
          if (DeclFile != 0
              && headers.getFileDirFlavor(DeclFile)
                  == DirectoryLookup::NormalHeaderDir) {
            cerr << "Found global user declarator " << II->getName() << endl;
          }
        }
      }
    }

    return MinimalAction::ActOnDeclarator(S, D, LastInGroup);
  }
};


int main(int argc, char* argv[])
{
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  PPContext context;

  // Add header search directories
  InitHeaderSearch init(context.headers);
  init.AddDefaultSystemIncludePaths(context.opts);
  init.Realize();

  // Add input file
  const FileEntry* File = context.fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File, SourceLocation());
  context.pp.EnterMainSourceFile();

  // Parse it
  IdentifierTable tab(context.opts);
  MyAction action(tab, context.pp);
  Parser p(context.pp, action);
  p.ParseTranslationUnit();
}
