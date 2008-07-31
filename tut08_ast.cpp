#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>
#include <map>
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
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/StmtVisitor.h"

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

    switch (DiagLevel) {
    default: assert(0 && "Unknown diagnostic type!");
    case Diagnostic::Note:    return; cerr << "note: "; break;
    case Diagnostic::Warning: return; cerr << "warning: "; break;
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
    FileManager& fm,
    bool isFramework = false)
{
  // If the directory exists, add it.
  if (const DirectoryEntry *DE = fm.getDirectory(&path[0], 
                                                 &path[0]+path.size())) {
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

// XXX: use some llvm map?
typedef map<VarDecl*, vector<DeclRefExpr*> > UsageMap;

// XXX: hack
map<DeclRefExpr*, FunctionDecl*> enclosing;

class FindUsages : public StmtVisitor<FindUsages> {
  UsageMap& uses;
  FunctionDecl* fd;
public:
  FindUsages(UsageMap& um) : uses(um) {}

  void VisitDeclRefExpr(DeclRefExpr* expr) {
    if (VarDecl* vd = dyn_cast<VarDecl>(expr->getDecl())) {
      UsageMap::iterator im = uses.find(vd);
      if (im != uses.end()) {
        im->second.push_back(expr);
        enclosing[expr] = fd;
      }
    }
  }

  void VisitStmt(Stmt* stmt) {
    Stmt::child_iterator CI, CE = stmt->child_end();
    for (CI = stmt->child_begin(); CI != CE; ++CI) {
      Visit(*CI);
    }
  }

  void process(FunctionDecl* fd) {
    this->fd = fd;
    Visit(fd->getBody());
  }
};


class MyASTConsumer : public ASTConsumer {
  SourceManager *sm;
  vector<FunctionDecl*> functions;
  vector<VarDecl*> globals;
public:
  virtual void Initialize(ASTContext &Context) {
    sm = &Context.getSourceManager();
  }

  virtual void HandleTopLevelDecl(Decl *D) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->isFileVarDecl() && VD->getStorageClass() != VarDecl::Extern) {
        // XXX: does not store c in `int b, c;`.
        globals.push_back(VD);
      }
    } else if (FunctionDecl* FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->getBody() != 0) {
        // XXX: This also collects functions from header files.
        // This makes us slower than necesary, but doesn't change the results.
        functions.push_back(FD);
      }
    }
  }

  virtual void HandleTranslationUnit(TranslationUnit& tu) {
    // called when everything is done

    UsageMap uses;
    for (int i = 0; i < globals.size(); ++i) {
      uses[globals[i]] = vector<DeclRefExpr*>();
    }

    FindUsages fu(uses);
    for (int i = 0; i < functions.size(); ++i) {
      fu.process(functions[i]);
    }

    for (int i = 0; i < globals.size(); ++i) {
      VarDecl* VD = globals[i];
      FullSourceLoc loc(VD->getLocation(), *sm);
      bool isStatic = VD->getStorageClass() == VarDecl::Static;

      cout << loc.getSourceName() << ": "
           << (isStatic?"static ":"") << VD->getName() << "\n";

      for (int j = 0; j < uses[VD].size(); ++j) {
        DeclRefExpr* dre = uses[VD][j];
        cout << "  " << enclosing[dre]->getName() << "\n";
      }
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
static llvm::cl::opt<string> dummy("o", llvm::cl::desc("dummy for gcc compat"));


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
  for (int i = 0; i < I_dirs.size(); ++i) {
    cerr << "adding " << I_dirs[i] << endl;
    addIncludePath(dirs, I_dirs[i], DirectoryLookup::NormalHeaderDir, fm);
  }

  // This specifies where in `dirs` the system headers start. Quoted includes
  // are searched for in all paths, angled includes only in the system headers.
  // -I can point to the direction of e.g. Carbon.h, which is usually included
  // in angle brackets. So we add everything to the system headers.
  unsigned systemDirIdx = 0;

  // system headers
  addIncludePath(dirs, "/usr/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/local/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/i686-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/usr/lib/gcc/powerpc-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, fm);
  addIncludePath(dirs, "/Library/Frameworks",
      DirectoryLookup::SystemHeaderDir, fm, /*isFramework=*/true);
  addIncludePath(dirs, "/System/Library/Frameworks",
      DirectoryLookup::SystemHeaderDir, fm, /*isFramework=*/true);

  bool noCurDirSearch = true;  // do not search in the current directory
  headers.SetSearchPaths(dirs, systemDirIdx, noCurDirSearch);


  // Add defines passed in through parameters
  vector<char> predefineBuffer;
  for (int i = 0; i < D_macros.size(); ++i) {
    cerr << "defining " << D_macros[i] << endl;
    DefineBuiltinMacro(predefineBuffer, D_macros[i].c_str());
  }
  predefineBuffer.push_back('\0');
  pp.setPredefines(&predefineBuffer[0]);


  // Add input file

  const FileEntry* File = fm.getFile(InputFilename);
  if (!File) {
    cerr << "Failed to open \'" << InputFilename << "\'" << endl;
    return EXIT_FAILURE;
  }
  sm.createMainFileID(File, SourceLocation());


  // Parse it

  cout << "<h2><code>" << InputFilename << "</code></h2>" << endl << endl;
  cout << "<pre><code>";

  ASTConsumer* c = new MyASTConsumer;
  ParseAST(pp, c);  // deletes c

  cout << "</pre></code>" << endl << endl;

  delete target;

  cout << endl;

  unsigned NumDiagnostics = diags.getNumDiagnostics();
  
  if (NumDiagnostics)
    fprintf(stderr, "%d diagnostic%s generated.\n", NumDiagnostics,
            (NumDiagnostics == 1 ? "" : "s"));
}
