#include "Interpreter.h"
#include "Utils.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h" // llvm_shutdown
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h" // llvm::Initialize*

static void LLVMErrorHandler(void *UserData, const char *Message,
                             bool GenCrashDiag) {
  auto &Diags = *static_cast<clang::DiagnosticsEngine *>(UserData);
  Diags.Report(clang::diag::err_fe_error_backend) << Message;
  llvm::sys::RunInterruptHandlers();
  exit(GenCrashDiag ? 70 : 1);
}

llvm::ExitOnError ExitOnErr;

static llvm::cl::opt<bool>
    wrap("w", llvm::cl::desc("wrap statement to declaration"));

static llvm::cl::list<std::string>
    IncludePaths("I", llvm::cl::desc("specify include paths"),
                 llvm::cl::ZeroOrMore);

static llvm::cl::list<std::string> Libs("L", llvm::cl::desc("load given libs"),
                                        llvm::cl::ZeroOrMore);

static llvm::cl::opt<std::string> inputFile(llvm::cl::Positional,
                                            llvm::cl::desc("<input file>"),
                                            llvm::cl::Required);

int main(int argc, const char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto CI = ExitOnErr(clang::Interpreter::CreateCI());

  llvm::install_fatal_error_handler(LLVMErrorHandler,
                                    static_cast<void *>(&CI->getDiagnostics()));
  // llvm::report_fatal_error("test:");

  auto Interp = ExitOnErr(clang::Interpreter::create(std::move(CI)));

  Interp->enablerWrapInput(wrap);

  Interp->AddIncludePath(".");
  for (size_t i = 0; i < IncludePaths.size(); i++) {
    Interp->AddIncludePath(IncludePaths[i]);
  }

  for (size_t i = 0; i < Libs.size(); i++) {
    if (clang::isDynamicLibrary(Libs[i])) {
      Interp->AddDynamicLib(Libs[i]);
    } else {
      Interp->AddStaticLib(Libs[i]);
    }
  }

  if (auto Err = Interp->ParseAndExecute(inputFile)) {
    llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "error: ");
    return 0;
  }

  llvm::remove_fatal_error_handler();
  llvm::llvm_shutdown();
  return 0;
}
