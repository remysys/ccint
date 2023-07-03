#ifndef LLVM_CLANG_TOOLS_CLANG_CCINT_INTERPRETER_H
#define LLVM_CLANG_TOOLS_CLANG_CCINT_INTERPRETER_H

#include "clang/AST/GlobalDecl.h"

#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/Support/Error.h"

#include <memory>
#include <vector>

namespace llvm {
class Module;

namespace orc {
class LLJIT;
class ThreadSafeContext;
} // namespace orc
} // namespace llvm

namespace clang {

class CompilerInstance;
class CCIntJIT;
class CCIntParser;

class Interpreter {
  bool m_WrapInput;
  std::vector<std::string> StaticLibVec;
  std::vector<std::string> DynamicLibVec;
  std::vector<std::string> HeaderPathVec;

  std::unique_ptr<llvm::orc::ThreadSafeContext> TSCtx;
  std::unique_ptr<CCIntParser> Parser;
  std::unique_ptr<CCIntJIT> Executor;
  Interpreter(std::unique_ptr<CompilerInstance> CI, llvm::Error &Err);

public:
  ~Interpreter();
  static llvm::Expected<std::unique_ptr<CompilerInstance>> CreateCI();
  static llvm::Expected<std::unique_ptr<Interpreter>>
  create(std::unique_ptr<CompilerInstance> CI);
  CompilerInstance *getCompilerInstance();
  std::unique_ptr<llvm::Module> getModule();

  void AddIncludePath(llvm::StringRef Path);
  void PrintIncludePath();

  void AddStaticLib(llvm::StringRef Path);
  void AddDynamicLib(llvm::StringRef Path);
  void AddHeaderPath(llvm::StringRef Path);

  llvm::Error Parse(llvm::StringRef FileName);

  llvm::Error Execute();

  llvm::Error ParseAndExecute(llvm::StringRef FileName) {
    if (auto Err = Parse(FileName)) {
      return Err;
    }
    return Execute();
  }

  bool isWrapInputEnabled() const { return m_WrapInput; }
  void enablerWrapInput(bool wrap = true) { m_WrapInput = wrap; }

  llvm::Expected<llvm::JITTargetAddress> getSymbolAddress() const;
};
} // namespace clang

#endif // LLVM_CLANG_TOOLS_CLANG_CCINT_INTERPRETER_H
