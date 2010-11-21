/*
# FIXME(thakis): Why is the clang headers include path not set by default?
./tut_rewrite -x c++ tut_rewrite.cpp \
    -I/Users/nico/src/llvm-2.8/tools/clang/include/ \
    `/Users/nico/src/llvm-2.8/Release/bin/llvm-config --cxxflags | \
        sed s/-fno-exceptions//` \
    -I/Users/nico/src/llvm-2.8/Release/lib/clang/2.8/include
*/
#include <iostream>

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
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Parse/ParseAST.h"

#include "clang/Rewrite/Rewriter.h"

// Possible approaches:
// 1. After parsing everything, find the Decl chain we want to rename, then
//    find all DeclRefExprs pointing to this chain, and rename these
// 2. Rename everything directly
// This uses approach 2.
class RenameFunctionVisitor :
    public clang::RecursiveASTVisitor<RenameFunctionVisitor> {
  clang::Rewriter& rewriter_;
 public:
  RenameFunctionVisitor(clang::Rewriter& r) : rewriter_(r) {}
  bool VisitFunctionDecl(clang::FunctionDecl *D);
  bool VisitDeclRefExpr(clang::DeclRefExpr *E);
};

bool RenameFunctionVisitor::VisitFunctionDecl(clang::FunctionDecl *D) {
fprintf(stderr, "visiting FD %s\n", std::string(D->getName()).c_str());
  return true;
}

bool RenameFunctionVisitor::VisitDeclRefExpr(clang::DeclRefExpr* E) {
  if (clang::FunctionDecl* D = dyn_cast<clang::FunctionDecl>(E->getDecl())) {
fprintf(stderr, "visiting function DRE %s %p %p\n",
        std::string(D->getName()).c_str(), D, D->getCanonicalDecl());
  }
  return true;
}

class RenameFunctionConsumer : public clang::ASTConsumer {
  clang::Rewriter rewriter_;
  clang::ASTContext* context_;
  clang::Diagnostic &diags_;
 public:
  RenameFunctionConsumer(clang::Diagnostic& d) : diags_(d) {}
  void HandleTopLevelSingleDecl(clang::Decl *D);

  // ASTConsumer
  virtual void Initialize(clang::ASTContext &Context);
  virtual void HandleTopLevelDecl(clang::DeclGroupRef D) {
    for (clang::DeclGroupRef::iterator I = D.begin(), E = D.end();
        I != E;
        ++I)
      HandleTopLevelSingleDecl(*I);
  }
  virtual void HandleTranslationUnit(clang::ASTContext &C);
};

void RenameFunctionConsumer::Initialize(clang::ASTContext &Context) {
  context_ = &Context;
  rewriter_.setSourceMgr(Context.getSourceManager(),
                         Context.getLangOptions());
}

void RenameFunctionConsumer::HandleTopLevelSingleDecl(clang::Decl *D) {
  if (diags_.hasErrorOccurred())
    return;

  clang::SourceLocation Loc = D->getLocation();
  Loc = context_->getSourceManager().getInstantiationLoc(Loc);
  
  // Only rewrite stuff in the main file for now.
  // FIXME(thakis): Will want to rewrite stuff in headers too at some point.
  if (!context_->getSourceManager().isFromMainFile(Loc))
    return;

  // We might want to process this decl. We will probably want to check if
  // it's a function decl (this covers c++ methods too, but not objc
  // functions), and then recurse into all statements in the function's body.
  RenameFunctionVisitor visitor(rewriter_);
  visitor.TraverseDecl(D);
}

void RenameFunctionConsumer::HandleTranslationUnit(clang::ASTContext &C) {
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
    std::cerr << "No filename given" << std::endl;
    return EXIT_FAILURE;
  }

  // Create Preprocessor object
  clang::DiagnosticClient* diagClient =  // Owned by |diags|.
      new clang::TextDiagnosticPrinter(llvm::outs(), inv.getDiagnosticOpts());
  clang::Diagnostic diags(diagClient);

  // Called by CompilerInstance::createDiagnostics(), which does some stuff
  // we don't want (?). FIXME(thakis): Use CompilerInstance instead?
  clang::ProcessWarningOptions(diags, inv.getDiagnosticOpts());

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
    std::cerr << "Failed to open \'" << inv.getFrontendOpts().Inputs[0].second
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
  RenameFunctionConsumer astConsumer(diags);

  diagClient->BeginSourceFile(inv.getLangOpts(), &pp);
  clang::ParseAST(pp, &astConsumer, astContext, /*PrintStats=*/false);
  diagClient->EndSourceFile();
}

