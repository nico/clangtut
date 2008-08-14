#include <algorithm>
#include <iostream>
#include <fstream>
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
#include "clang/AST/TranslationUnit.h"

#include "llvm/System/Path.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/System/Signals.h"

#include "clang/Driver/TextDiagnosticPrinter.h"

#include "llvm/Config/config.h"

using namespace clang;


#include "PreprocessorContext.h"

// serializer {{{
class Serializer : public ASTConsumer {
  TranslationUnit* TU;
  const llvm::sys::Path FName;
public:
  Serializer(const llvm::sys::Path& F) : TU(0), FName(F) {}    
  
  ~Serializer() {
    EmitASTBitcodeFile(TU, FName);
  }

  virtual void InitializeTU(TranslationUnit &tu) {
    TU = &tu;
  }
};
// }}}

// include and define funcs {{{
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
// }}}

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
      if (*CI != 0) {
        Visit(*CI);
      }
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
  vector<pair<VarDecl*, bool> > globals;
  ostream& out;
public:
  MyASTConsumer(ostream& o) : out(o) {}

  virtual void Initialize(ASTContext &Context) {
    sm = &Context.getSourceManager();
  }

  virtual void HandleTopLevelDecl(Decl *D) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->isFileVarDecl()) {
        // XXX: does not store c in `int b, c;`.
        globals.push_back(make_pair(VD,
              VD->getStorageClass() != VarDecl::Extern));
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
      uses[globals[i].first] = vector<DeclRefExpr*>();
    }

    FindUsages fu(uses);
    for (int i = 0; i < functions.size(); ++i) {
      fu.process(functions[i]);
    }

    int count = 0;
    vector<DeclRefExpr*> allUses;
    for (int i = 0; i < globals.size(); ++i) {
      VarDecl* VD = globals[i].first;

      //allUses.append(uses[VD].begin(), uses[VD].end());
      allUses.insert(allUses.end(), uses[VD].begin(), uses[VD].end());

      if (globals[i].second)
        ++count;
    }

    out << count << " defines\n";
    for (int i = 0; i < globals.size(); ++i) {
      if (!globals[i].second) continue;

      VarDecl* VD = globals[i].first;
      FullSourceLoc loc(VD->getLocation(), *sm);
      bool isStatic = VD->getStorageClass() == VarDecl::Static;

      out
          //<< "<span class=\"global\">"
          << loc.getSourceName() << ":" << loc.getLogicalLineNumber() << " "
          << (isStatic?"static ":"") << VD->getName()
          //<< "  (" << uses[VD].size() << " local uses)"
          << "\n"
          //<< "</span>"
          ;
    }

    out << allUses.size() << " uses\n";
    //out << "<span class=\"uses\">";
    for (int j = 0; j < allUses.size(); ++j) {
      DeclRefExpr* dre = allUses[j];
      FunctionDecl* fd = enclosing[dre];
      FullSourceLoc loc(dre->getLocStart(), *sm);
      out
          //<< "  "
          << fd->getName() << ":"
          << loc.getLogicalLineNumber()
          << " " << dre->getDecl()->getName()
          << "\n";
    }
    //out << "</span>";
  }
};

static llvm::cl::list<std::string>
D_macros("D", llvm::cl::value_desc("macro"), llvm::cl::Prefix,
    llvm::cl::desc("Predefine the specified macro"));

static llvm::cl::list<std::string>
I_dirs("I", llvm::cl::value_desc("directory"), llvm::cl::Prefix,
    llvm::cl::desc("Add directory to include search path"));

static llvm::cl::list<string>
InputFilenames(llvm::cl::Positional, llvm::cl::desc("<input file>"),
    llvm::cl::OneOrMore);

//static llvm::cl::opt<llvm::sys::Path>
static llvm::cl::opt<string>
OutputFilename("o", llvm::cl::value_desc("outfile"),
    llvm::cl::desc("Name of output file"), llvm::cl::Required);

static llvm::cl::list<std::string> IgnoredParams(llvm::cl::Sink);


void addIncludesAndDefines(PPContext& c) {/*{{{*/
  // Add header search directories (C only, no C++ or ObjC)

  vector<DirectoryLookup> dirs;

  // user headers
  for (int i = 0; i < I_dirs.size(); ++i) {
    cerr << "adding " << I_dirs[i] << endl;
    addIncludePath(dirs, I_dirs[i], DirectoryLookup::NormalHeaderDir, c.fm);
  }

  // include paths {{{
  // This specifies where in `dirs` the system headers start. Quoted includes
  // are searched for in all paths, angled includes only in the system headers.
  // -I can point to the direction of e.g. Carbon.h, which is usually included
  // in angle brackets. So we add everything to the system headers.
  unsigned systemDirIdx = 0;

  // system headers
  addIncludePath(dirs, "/usr/include",
      DirectoryLookup::SystemHeaderDir, c.fm);
  addIncludePath(dirs, "/usr/local/include",
      DirectoryLookup::SystemHeaderDir, c.fm);
  addIncludePath(dirs, "/usr/lib/gcc/i686-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, c.fm);
  addIncludePath(dirs, "/usr/lib/gcc/powerpc-apple-darwin9/4.0.1/include",
      DirectoryLookup::SystemHeaderDir, c.fm);
  addIncludePath(dirs, "/Library/Frameworks",
      DirectoryLookup::SystemHeaderDir, c.fm, /*isFramework=*/true);
  addIncludePath(dirs, "/System/Library/Frameworks",
      DirectoryLookup::SystemHeaderDir, c.fm, /*isFramework=*/true);
  // }}}

  bool noCurDirSearch = false;  // search in the current directory
  c.headers.SetSearchPaths(dirs, systemDirIdx, noCurDirSearch);


  // Add defines passed in through parameters
  vector<char> predefineBuffer;
  for (int i = 0; i < D_macros.size(); ++i) {
    cerr << "defining " << D_macros[i] << endl;
    DefineBuiltinMacro(predefineBuffer, D_macros[i].c_str());
  }
  predefineBuffer.push_back('\0');
  c.pp.setPredefines(&predefineBuffer[0]);
}/*}}}*/

bool compile(ostream& out, const string& inFile)
{
  // Create Preprocessor object
  PPContext context;
  addIncludesAndDefines(context);


  // Add input file

  const FileEntry* File = context.fm.getFile(inFile);
  if (!File) {
    cerr << "Failed to open \'" << inFile << "\'" << endl;
    return false;
  }
  context.sm.createMainFileID(File, SourceLocation());

  // Serialize it
  //ASTConsumer* c = new Serializer(OutputPath);
  //ParseAST(context.pp, c);  // deletes c


  if (!out) {
    cerr << "Failed to open \'" << OutputFilename << "\'" << endl;
    return false;
  }
  //ostream& out = cout;

  //// Parse it

  out
      //<< "<h2><code>"
      << InputFilenames[0]
      //<< "</code></h2>" << endl
      << endl;
  //out << "<pre><code>";

  ASTConsumer* c = new MyASTConsumer(out);
  ParseAST(context.pp, c);
  delete c;

  //out << "</code></pre>" << endl << endl;

  out << endl;

  unsigned NumDiagnostics = context.diags.getNumDiagnostics();
  
  if (NumDiagnostics)
    fprintf(stderr, "%d diagnostic%s generated.\n", NumDiagnostics,
            (NumDiagnostics == 1 ? "" : "s"));

  return true;
}

void parseNameColonNumber(istream& in, string& nameOut, int& numOut)
{
  string dummy; in >> dummy;
  string::size_type p = dummy.find(":");
  nameOut = dummy.substr(0, p);
  numOut = atoi(dummy.substr(p + 1).c_str());
}

struct Define {
  string definingTU;
  string definingFile;
  int definingLine;
  bool isStatic;
  string name;
};

struct Use {
  string usingTU;
  string usingFunction;
  int usingLine;
  string name;
  Define* var;
};

bool link(const vector<string>& files)
{
  vector<Define> allDefines;
  vector<Use> allUses;
  for (int i = 0; i < files.size(); ++i) {
    ifstream in(files[i].c_str());
    if (!in) {
      cerr << "Failed to open \'" << files[i] << "\"\n";
      return false;
    }

    string inName, dummy;
    in >> inName;

    int numDefines; in >> numDefines >> dummy; in.get();
    for (int j = 0; j < numDefines; ++j) {
      Define d;
      d.definingTU = inName;
      parseNameColonNumber(in, d.definingFile, d.definingLine);
      in >> dummy;
      d.isStatic = dummy == "static";
      if (d.isStatic)
        in >> dummy;
      d.name = dummy;
      allDefines.push_back(d);
    }

    int numUses; in >> numUses >> dummy; in.get();
    for (int j = 0; j < numUses; ++j) {
      Use u;
      u.usingTU = inName;
      parseNameColonNumber(in, u.usingFunction, u.usingLine);
      in >> u.name;
      allUses.push_back(u);
    }
  }

  map<pair<string, string>, Define*> defineMap;
  for (int i = 0; i < allDefines.size(); ++i) {
    Define& d = allDefines[i];
    pair<string, string> key("", d.name);
    if (d.isStatic)
      key.first = d.definingTU;
    //cout << key.first << " " << key.second << endl;
    assert(defineMap.find(key) == defineMap.end());
    defineMap[key] = &d;
  }

  for (int i = 0; i < allUses.size(); ++i) {
    Use& u = allUses[i];
    Define* d = NULL;

    // look up static global
    if (defineMap.find(make_pair(u.usingTU, u.name)) != defineMap.end())
      d = defineMap[make_pair(u.usingTU, u.name)];

    // look up general global
    if (d == NULL && defineMap.find(make_pair("", u.name)) != defineMap.end()) {
      d = defineMap[make_pair("", u.name)];
    }

    if (d == NULL) {
      cout << "Unresolved global \"" << u.name << "\"\n";
      exit(1);
    }
    u.var = d;
  }

  for (int i = 0; i < allUses.size(); ++i) {
    Use& u = allUses[i];
    cout << u.name << ":: "
         << u.usingFunction << ":" << u.usingLine
         << " -> " << u.var->definingFile << ":" << u.var->definingLine
         << " (" << u.var->definingTU << ")"
         << endl;
  }
}

int main(int argc, char* argv[])
{
  llvm::cl::ParseCommandLineOptions(argc, argv, " globalcollect\n"
      "  This collects and prints global variables found in C programs.");
  llvm::sys::PrintStackTraceOnErrorSignal();

  if (!IgnoredParams.empty()) {
    cerr << "Ignoring the following parameters:";
    copy(IgnoredParams.begin(), IgnoredParams.end(),
        ostream_iterator<string>(cerr, " "));
  }

  enum { COMPILE, LINK } mode = COMPILE;

  llvm::sys::Path OutputPath(OutputFilename);
  if (InputFilenames.size() > 1 || OutputPath.getSuffix() != "o")
    mode = LINK;

  if (OutputPath.getSuffix() !=  "o" && mode == COMPILE) {
    cerr << "Need to compile to a .o file" << endl;
    return 1;
  }

  if (mode == COMPILE) {
    ofstream out(OutputFilename.c_str());
    compile(out, InputFilenames[0]);
    out.close();
  } else {
    link(InputFilenames);
  }
}
