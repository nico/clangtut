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
#include "clang/AST/DeclGroup.h"

#include "clang/Parse/ParseAST.h"

#include "clang/Rewrite/Rewriter.h"

class RenameMethodConsumer : public clang::ASTConsumer {
  clang::Rewriter rewriter_;
  clang::ASTContext* context_;
  clang::Diagnostic &diags_;
 public:
  RenameMethodConsumer(clang::Diagnostic& d) : diags_(d) {}
  void HandleTopLevelSingleDecl(clang::Decl *D);

  // ASTConsumer
  virtual void Initialize(clang::ASTContext &Context);
  virtual void HandleTopLevelDecl(clang::DeclGroupRef D) {
    for (clang::DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I)
      HandleTopLevelSingleDecl(*I);
  }
  virtual void HandleTranslationUnit(clang::ASTContext &C);
};

void RenameMethodConsumer::Initialize(clang::ASTContext &Context) {
  context_ = &Context;
  rewriter_.setSourceMgr(Context.getSourceManager(), Context.getLangOptions());
}

void RenameMethodConsumer::HandleTopLevelSingleDecl(clang::Decl *D) {
  if (diags_.hasErrorOccurred())
    return;

  clang::SourceLocation Loc = D->getLocation();
  Loc = context_->getSourceManager().getInstantiationLoc(Loc);
  
  // Only rewrite stuff in the main file for now.
  // FIXME(thakis): Will want to rewrite stuff in headers too at some point.
  if (!context_->getSourceManager().isFromMainFile(Loc))
    return;

  // We might want to process this decl. We will probably want to check if
  // it's a function decl (this covers c++ methods too, but not objc functions),
  // and then recurse into all statements in the function's body.
}

void RenameMethodConsumer::HandleTranslationUnit(clang::ASTContext &C) {
}

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
      new clang::TextDiagnosticPrinter(llvm::outs(), inv.getDiagnosticOpts());
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
  // Do _not_ call EnterMainSourceFile() -- the parser does this

  // Register built-ins (__builtin_strstr etc)
  pp.getBuiltinInfo().InitializeBuiltins(pp.getIdentifierTable(),
                                         pp.getLangOptions().NoBuiltin);

  // Parse it
  clang::ASTContext astContext(
      pp.getLangOptions(),
      pp.getSourceManager(),
      pp.getTargetInfo(),
      pp.getIdentifierTable(),
      pp.getSelectorTable(),
      pp.getBuiltinInfo(),
      /*size_reserve=*/0);
  RenameMethodConsumer astConsumer(diags);

  diagClient->BeginSourceFile(inv.getLangOpts(), &pp);
  clang::ParseAST(pp, &astConsumer, astContext, /*PrintStats=*/true);
  diagClient->EndSourceFile();
}

