
#include <string>

#include "llvm/Config/config.h"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/FileManager.h"

#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Driver/TextDiagnosticPrinter.h"

struct PPContext {
  // Takes ownership of client.
  PPContext(DiagnosticClient* client = 0,
            const std::string& triple = LLVM_HOSTTRIPLE)
    : diagClient(client == 0?new TextDiagnosticPrinter:client),
      diags(diagClient),
      target(TargetInfo::CreateTargetInfo(triple)),
      headers(fm),
      pp(diags, opts, *target, sm, headers)
  {}

  ~PPContext()
  {
    delete diagClient;
    delete target;
  }

  DiagnosticClient* diagClient;
  Diagnostic diags;
  LangOptions opts;
  TargetInfo* target;
  SourceManager sm;
  FileManager fm;
  HeaderSearch headers;
  Preprocessor pp;
};
