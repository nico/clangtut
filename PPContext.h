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

#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

struct PPContext {
  // Takes ownership of client.
  PPContext(clang::DiagnosticClient* client = 0,
            const std::string& triple = LLVM_HOSTTRIPLE)
    : diagClient(client == 0 ?
          new clang::TextDiagnosticPrinter(llvm::outs(), diagOptions) :
          client),
      // clang::Diagnostic takes ownership of its client.
      diags(diagClient),
      sm(diags),
      headers(fm)
  {
    targetOptions.Triple = triple;
    target.reset(clang::TargetInfo::CreateTargetInfo(diags, targetOptions));
    pp.reset(new clang::Preprocessor(diags, opts, *target, sm, headers));
  }

  ~PPContext()
  {
  }

  clang::DiagnosticOptions diagOptions;
  clang::DiagnosticClient* diagClient;  // Owned by |diags|.
  clang::Diagnostic diags;
  clang::LangOptions opts;

  clang::TargetOptions targetOptions;
  llvm::OwningPtr<clang::TargetInfo> target;

  clang::FileManager fm;
  clang::SourceManager sm;

  clang::HeaderSearch headers;
  llvm::OwningPtr<clang::Preprocessor> pp;
};

#endif
