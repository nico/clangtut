#include <algorithm>
#include <iostream>
#include <iterator>
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

#include "llvm/Support/CommandLine.h"

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

    cerr << endl;

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


// This function is already in Preprocessor.cpp and clang.cpp. It should be
// in a library.

// Append a #define line to Buf for Macro.  Macro should be of the form XXX,
// in which case we emit "#define XXX 1" or "XXX=Y z W" in which case we emit
// "#define XXX Y z W".  To get a #define with no value, use "XXX=".
static void DefineBuiltinMacro(std::vector<char> &Buf, const char *Macro,
                               const char *Command = "#define ") {
  Buf.insert(Buf.end(), Command, Command+strlen(Command));
  if (const char *Equal = strchr(Macro, '=')) {
    // Turn the = into ' '.
    Buf.insert(Buf.end(), Macro, Equal);
    Buf.push_back(' ');
    Buf.insert(Buf.end(), Equal+1, Equal+strlen(Equal));
  } else {
    // Push "macroname 1".
    Buf.insert(Buf.end(), Macro, Macro+strlen(Macro));
    Buf.push_back(' ');
    Buf.push_back('1');
  }
  Buf.push_back('\n');
}

class MyASTConsumer : public ASTConsumer {
public:
  virtual void HandleTopLevelDecl(Decl *D) {
    // XXX: does not print c in `int b, c;`.
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->isFileVarDecl() && VD->getStorageClass() != VarDecl::Extern)
        cerr << "Read top-level variable decl: '" << VD->getName() << "'\n";
    }
  }
};


static llvm::cl::list<std::string>
D_macros("D", llvm::cl::value_desc("macro"), llvm::cl::Prefix,
    llvm::cl::desc("Predefine the specified macro"));

static llvm::cl::list<std::string>
I_dirs("I", llvm::cl::value_desc("directory"), llvm::cl::Prefix,
    llvm::cl::desc("Add directory to include search path"));

static llvm::cl::opt<string>
InputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"),
    llvm::cl::Required);

static llvm::cl::list<std::string> IgnoredParams(llvm::cl::Sink);


int main(int argc, char* argv[])
{
  llvm::cl::ParseCommandLineOptions(argc, argv, " globalcollect\n"
      "  This collects and prints global variables found in C programs.");

  if (!IgnoredParams.empty()) {
    cerr << "Ignoring the following parameters:";
    copy(IgnoredParams.begin(), IgnoredParams.end(),
        ostream_iterator<string>(cerr, " "));
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
  for (int i = 0; i < I_dirs.size(); ++i)
    addIncludePath(dirs, I_dirs[i], DirectoryLookup::NormalHeaderDir, fm);

  unsigned systemDirIdx = dirs.size();

  // system headers
  addIncludePath(dirs, "/usr/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/i686-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/powerpc-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);

  bool noCurDirSearch = true;  // do not search in the current directory
  headers.SetSearchPaths(dirs, systemDirIdx, noCurDirSearch);


  // Add defines passed in through parameters
  vector<char> predefineBuffer;
  for (int i = 0; i < D_macros.size(); ++i)
    DefineBuiltinMacro(predefineBuffer, D_macros[i].c_str());
  predefineBuffer.push_back('\0');
  pp.setPredefines(&predefineBuffer[0]);


  // Add input file

  const FileEntry* File = fm.getFile(InputFilename);
  if (!File) {
    cerr << "Failed to open \'" << InputFilename << "\'" << endl;
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File, SourceLocation());
  //pp.EnterMainSourceFile();


  // Parse it

  ASTConsumer* c = new MyASTConsumer;
  ParseAST(pp, c);  // deletes c

  delete target;

  unsigned NumDiagnostics = diags.getNumDiagnostics();
  
  if (NumDiagnostics)
    fprintf(stderr, "%d diagnostic%s generated.\n", NumDiagnostics,
            (NumDiagnostics == 1 ? "" : "s"));
}
