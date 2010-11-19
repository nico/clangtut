#ifndef PP_CONTEXT
#define PP_CONTEXT

#include <string>

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

// FIXME: argh, this depends on libclangDriver!
// FIXME: _and_ libclangSerialization!
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

struct PPContext {
  // Takes ownership of client.
  PPContext(clang::CompilerInvocation& i)
    : inv(i),
      diagClient(new clang::TextDiagnosticPrinter(llvm::outs(),
                                                  inv.getDiagnosticOpts())),
      // clang::Diagnostic takes ownership of its client.
      diags(diagClient),
      target(clang::TargetInfo::CreateTargetInfo(diags, inv.getTargetOpts())),
      sm(diags),
      headers(fm),
      pp(new clang::Preprocessor(
            diags, inv.getLangOpts(), *target, sm, headers))
  {
  }

  ~PPContext()
  {
  }

  clang::CompilerInvocation& inv;

  clang::DiagnosticClient* diagClient;  // Owned by |diags|.
  clang::Diagnostic diags;

  llvm::OwningPtr<clang::TargetInfo> target;

  clang::FileManager fm;
  clang::SourceManager sm;

  clang::HeaderSearch headers;
  llvm::OwningPtr<clang::Preprocessor> pp;
};

#endif

#include <iostream>
using namespace std;

#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/HeaderSearchOptions.h"
#include "clang/Frontend/PreprocessorOptions.h"
#include "clang/Frontend/Utils.h"

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
  PPContext context(inv);

  // Add header search directories
  clang::ApplyHeaderSearchOptions(
      context.headers, inv.getHeaderSearchOpts(),
      inv.getLangOpts(), context.target->getTriple());

  // Add system default defines
  clang::InitializePreprocessor(*context.pp, inv.getPreprocessorOpts(),
    inv.getHeaderSearchOpts(), inv.getFrontendOpts());

  // Add input file
  const clang::FileEntry* File =
    context.fm.getFile(inv.getFrontendOpts().Inputs[0].second);
  if (!File) {
    cerr << "Failed to open \'" << inv.getFrontendOpts().Inputs[0].second
         << "\'";
    return EXIT_FAILURE;
  }
  context.sm.createMainFileID(File);
  context.pp->EnterMainSourceFile();

  // Parse it
  clang::Token Tok;
  context.diagClient->BeginSourceFile(inv.getLangOpts(), context.pp.get());
  do {
    context.pp->Lex(Tok);
    if (context.diags.hasErrorOccurred())
      break;
    context.pp->DumpToken(Tok);
    cerr << endl;
  } while (Tok.isNot(clang::tok::eof));
  context.diagClient->EndSourceFile();
}
