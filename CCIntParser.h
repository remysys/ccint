#ifndef LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_PARSER_H
#define LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_PARSER_H

#include "clang/AST/GlobalDecl.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <list>
#include <memory>
namespace llvm {
class LLVMContext;
class Module;
} // namespace llvm

namespace clang {
class ASTConsumer;
class CompilerInstance;
class CCIntAction;
class Parser;

class CCIntParser {
  std::unique_ptr<CCIntAction> Act;
  std::shared_ptr<CompilerInstance> CI;
  std::shared_ptr<Parser> P;
  llvm::StringRef MangledName;
  std::unique_ptr<llvm::Module> TheModule;

public:
  CCIntParser(std::unique_ptr<CompilerInstance> Instance,
              llvm::LLVMContext &LLVMCtx, llvm::Error &Err);
  ~CCIntParser();

  CompilerInstance *getCI() { return CI.get(); }
  std::unique_ptr<llvm::Module> getModule() { return std::move(TheModule); }

  llvm::Error Parse(llvm::StringRef FileName, bool Wrap);

  llvm::StringRef GetMangledName() const;
  std::string WrapInput(const std::string &Code);
};
} // end namespace clang

#endif // LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_PARSER_H
