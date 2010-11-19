#include <iostream>
#include <string>
using namespace std;

#include "llvm/Config/config.h"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/FileManager.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Frontend/PreprocessorOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

#include "clang/Parse/ParseAST.h"

int main(int argc, char* argv[])
{
  // Parse clang args.
  clang::DiagnosticOptions bootstrapDiagOptions;
  clang::Diagnostic bootstrapDiag(
      new clang::TextDiagnosticPrinter(llvm::outs(), bootstrapDiagOptions));
  clang::CompilerInvocation inv;
  clang::CompilerInvocation::CreateFromArgs(
      inv,
      const_cast<const char**>(argv + 1),
      const_cast<const char**>(argv) + argc,
      bootstrapDiag);
  if (inv.getFrontendOpts().Inputs.size() < 1 ||
      inv.getFrontendOpts().Inputs[0].second == "-") {
    cerr << "No filename given" << endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  clang::DiagnosticClient* diagClient =  // Owned by |diags|.
      new clang::TextDiagnosticPrinter(llvm::outs(),
                                       inv.getDiagnosticOpts());
  clang::Diagnostic diags(diagClient);

  llvm::OwningPtr<clang::TargetInfo> target(
      clang::TargetInfo::CreateTargetInfo(diags, inv.getTargetOpts()));

  clang::FileManager fm;
  clang::SourceManager sm(diags);

  clang::HeaderSearch headers(fm);
  clang::Preprocessor pp(diags, inv.getLangOpts(), *target, sm, headers);

  // Add header search directories
  clang::ApplyHeaderSearchOptions(
      headers, inv.getHeaderSearchOpts(),
      inv.getLangOpts(), target->getTriple());

  // Add system default defines
  clang::InitializePreprocessor(pp, inv.getPreprocessorOpts(),
    inv.getHeaderSearchOpts(), inv.getFrontendOpts());

  // Add input file
  const clang::FileEntry* File =
    fm.getFile(inv.getFrontendOpts().Inputs[0].second);
  if (!File) {
    cerr << "Failed to open \'" << inv.getFrontendOpts().Inputs[0].second
         << "\'";
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File);
  // Do _not_ call EnterMainSourceFile() -- the parser does this.

  // Parse it
  clang::IdentifierTable identifierTable(inv.getLangOpts());
  clang::SelectorTable selectorTable;

  clang::Builtin::Context builtinContext(*target);
  clang::ASTContext astContext(
      inv.getLangOpts(),
      sm,
      *target,
      identifierTable,
      selectorTable,
      builtinContext,
      /*size_reserve=*/0);
  clang::ASTConsumer astConsumer;

  diagClient->BeginSourceFile(inv.getLangOpts(), &pp);
  clang::ParseAST(pp, &astConsumer, astContext, /*PrintStats=*/true);
  diagClient->EndSourceFile();
}

