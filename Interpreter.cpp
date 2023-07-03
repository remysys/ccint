#include "Interpreter.h"
#include "CCIntJIT.h"
#include "CCIntParser.h"

#include "clang/AST/ASTContext.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Job.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm/IR/Module.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Host.h"
#include <memory>

#include <clang/AST/DeclVisitor.h>

using namespace clang;

namespace {

static llvm::Expected<const llvm::opt::ArgStringList *>
GetCC1Arguments(DiagnosticsEngine *Diagnostics,
                driver::Compilation *Compilation) {
  const driver::JobList &Jobs = Compilation->getJobs();

  auto IsCC1Command = [](const driver::Command &Cmd) {
    return StringRef(Cmd.getCreator().getName()) == "clang";
  };

  auto IsSrcFile = [](const driver::InputInfo &II) {
    return isSrcFile(II.getType());
  };

  llvm::SmallVector<const driver::Command *, 1> CC1Jobs;
  for (const driver::Command &Job : Jobs)
    if (IsCC1Command(Job) && llvm::all_of(Job.getInputInfos(), IsSrcFile))
      CC1Jobs.push_back(&Job);

  if (CC1Jobs.empty() || (CC1Jobs.size() > 1)) {
    return llvm::createStringError(llvm::errc::not_supported,
                                   "driver initialization failed");
  }

  return &CC1Jobs[0]->getArguments();
}

static llvm::Expected<std::unique_ptr<CompilerInstance>>
CreateCIInternal(const llvm::opt::ArgStringList &Argv) {
  std::unique_ptr<CompilerInstance> CI(new CompilerInstance());
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  auto PCHOps = CI->getPCHContainerOperations();
  PCHOps->registerWriter(std::make_unique<ObjectFilePCHContainerWriter>());
  PCHOps->registerReader(std::make_unique<ObjectFilePCHContainerReader>());

  /*
  for (auto it = Argv.begin(); it != Argv.end(); ++it) {
    std::cout << *it << std::endl;
  }
  */

  /*
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
  */

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);

  bool Success = CompilerInvocation::CreateFromArgs(
      CI->getInvocation(), llvm::makeArrayRef(Argv.begin(), Argv.size()),
      Diags);

  if (CI->getHeaderSearchOpts().UseBuiltinIncludes &&
      CI->getHeaderSearchOpts().ResourceDir.empty()) {
    CI->getHeaderSearchOpts().ResourceDir =
        CompilerInvocation::GetResourcesPath(Argv[0], nullptr);
  }

  CI->createDiagnostics();
  if (!CI->hasDiagnostics())
    return llvm::createStringError(llvm::errc::not_supported,
                                   "initialization failed. "
                                   "unable to create diagnostics engine");

  DiagsBuffer->FlushDiagnostics(CI->getDiagnostics());
  if (!Success) {
    CI->getDiagnosticClient().finish();
    return llvm::createStringError(llvm::errc::not_supported,
                                   "initialization failed. "
                                   "unable to flush diagnostics");
  }

  CI->getCodeGenOpts().ClearASTBeforeBackend = false;
  CI->getFrontendOpts().DisableFree = false;
  CI->getCodeGenOpts().DisableFree = false;

  return std::move(CI);
}

} // anonymous namespace
llvm::Expected<std::unique_ptr<CompilerInstance>> Interpreter::CreateCI() {
  std::vector<const char *> ClangArgv;
  std::string MainExecutableName =
      llvm::sys::fs::getMainExecutable(nullptr, nullptr);

  ClangArgv.insert(ClangArgv.begin(), MainExecutableName.c_str());

  ClangArgv.insert(ClangArgv.begin() + 1, "-c");
  ClangArgv.push_back("-x");
  ClangArgv.push_back("c++");
  ClangArgv.push_back("-D_GLIBCXX_USE_CXX11_ABI=0");

  ClangArgv.push_back("<input>");

  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts =
      CreateAndPopulateDiagOpts(ClangArgv);
  TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);

  driver::Driver Driver(ClangArgv[0], llvm::sys::getProcessTriple(), Diags);
  Driver.setCheckInputsExist(false);
  llvm::ArrayRef<const char *> RF = llvm::makeArrayRef(ClangArgv);
  std::unique_ptr<driver::Compilation> Compilation(Driver.BuildCompilation(RF));

  if (Compilation->getArgs().hasArg(driver::options::OPT_v))
    Compilation->getJobs().Print(llvm::errs(), "\n", false);

  auto ErrOrCC1Args = GetCC1Arguments(&Diags, Compilation.get());
  if (auto Err = ErrOrCC1Args.takeError())
    return std::move(Err);

  return CreateCIInternal(**ErrOrCC1Args);
}

Interpreter::Interpreter(std::unique_ptr<CompilerInstance> CI, llvm::Error &Err)
    : m_WrapInput(false) {
  llvm::ErrorAsOutParameter EAO(&Err);
  auto LLVMCtx = std::make_unique<llvm::LLVMContext>();
  TSCtx = std::make_unique<llvm::orc::ThreadSafeContext>(std::move(LLVMCtx));
  Parser =
      std::make_unique<CCIntParser>(std::move(CI), *TSCtx->getContext(), Err);
}

Interpreter::~Interpreter() {}

llvm::Expected<std::unique_ptr<Interpreter>>
Interpreter::create(std::unique_ptr<CompilerInstance> CI) {
  llvm::Error Err = llvm::Error::success();
  auto Interp =
      std::unique_ptr<Interpreter>(new Interpreter(std::move(CI), Err));
  if (Err)
    return std::move(Err);
  return std::move(Interp);
}

CompilerInstance *Interpreter::getCompilerInstance() { return Parser->getCI(); }

std::unique_ptr<llvm::Module> Interpreter::getModule() {
  return Parser->getModule();
}

llvm::Error Interpreter::Parse(llvm::StringRef FileName) {
  return Parser->Parse(FileName, isWrapInputEnabled());
}

llvm::Error Interpreter::Execute() {
  if (!Executor) {
    const clang::TargetInfo &TI = getCompilerInstance()->getTarget();

    llvm::Error Err = llvm::Error::success();
    Executor = std::make_unique<CCIntJIT>(*TSCtx, Err, TI);

    if (Err)
      return Err;

    for (auto &Path : StaticLibVec) {
      if (Err = Executor->AddStaticLib(Path)) {
        return Err;
      }
    }

    for (auto &Path : DynamicLibVec) {
      if (Err = Executor->AddDynamicLib(Path)) {
        return Err;
      }
    }

    if (Err = Executor->addModule(std::move(getModule()))) {
      return Err;
    }

    if (Err = Executor->runCtors()) {
      return Err;
    }
  }

  auto Symbol = getSymbolAddress();
  if (!Symbol) {
    return Symbol.takeError();
  }

  int (*fp)() = reinterpret_cast<int (*)()>(Symbol.get());
  int ret = fp();

  return llvm::Error::success();
}

llvm::Expected<llvm::JITTargetAddress> Interpreter::getSymbolAddress() const {
  if (!Executor) {
    return llvm::createStringError(llvm::errc::not_supported,
                                   "operation failed. no execution engine");
  }

  llvm::StringRef MangledName = Parser->GetMangledName();
  return Executor->getSymbolAddress(MangledName);
}

void Interpreter::AddIncludePath(llvm::StringRef Path) {

  CompilerInstance *CI = getCompilerInstance();
  HeaderSearchOptions &hso = CI->getHeaderSearchOpts();
  for (const HeaderSearchOptions::Entry &E : hso.UserEntries) {
    if ((E.Path == Path)) {
      return;
    }
  }

  hso.AddPath(Path, frontend::Angled, false, false);
}

void Interpreter::PrintIncludePath() {
  CompilerInstance *CI = getCompilerInstance();
  HeaderSearchOptions &hso = CI->getHeaderSearchOpts();

  for (size_t i = 0; i < hso.UserEntries.size(); ++i) {
    llvm::outs() << hso.UserEntries[i].Path << "\n";
  }
}

void Interpreter::AddStaticLib(llvm::StringRef Path) {
  StaticLibVec.push_back(Path.str());
}

void Interpreter::AddDynamicLib(llvm::StringRef Path) {
  DynamicLibVec.push_back(Path.str());
}

void Interpreter::AddHeaderPath(llvm::StringRef Path) {
  HeaderPathVec.push_back(Path.str());
}
