#include <iostream>
using namespace std;

#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Frontend/PreprocessorOptions.h"
#include "clang/Frontend/Utils.h"

#include "clang/Basic/IdentifierTable.h"
#include "clang/Parse/Parser.h"

#include "PPContext.h"

int main(int argc, char* argv[])
{
  if (argc != 2) {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  PPContext context;

  // Add header search directories
  clang::HeaderSearchOptions headersOpts;
  clang::ApplyHeaderSearchOptions(
      context.headers, headersOpts, context.opts, context.target->getTriple());

  // Add system default defines
  clang::PreprocessorOptions ppOpts;
  clang::FrontendOptions feOpts;
  clang::InitializePreprocessor(*context.pp, ppOpts, headersOpts, feOpts);

  // Add input file
  const clang::FileEntry* File = context.fm.getFile(argv[1]);
  if (!File) {
    cerr << "Failed to open \'" << argv[1] << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File);
  context.pp->EnterMainSourceFile();


  // Parse it
  clang::IdentifierTable tab(context.opts);
  clang::MinimalAction action(tab);
  Parser p(context.pp.get(), action);
  p.ParseTranslationUnit();

  tab.PrintStats();
}
