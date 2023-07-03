#ifndef LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_JIT_H
#define LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_JIT_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include <memory>

namespace llvm {
class Error;
namespace orc {
class LLJIT;
class ThreadSafeContext;
} // namespace orc
} // namespace llvm

namespace clang {

class TargetInfo;

class CCIntJIT {
  std::unique_ptr<llvm::orc::LLJIT> Jit;
  llvm::orc::ThreadSafeContext &TSCtx;

  llvm::DenseMap<const llvm::Module *, llvm::orc::ResourceTrackerSP>
      ResourceTrackers;

public:
  CCIntJIT(llvm::orc::ThreadSafeContext &TSC, llvm::Error &Err,
           const clang::TargetInfo &TI);
  ~CCIntJIT();

  llvm::Error addModule(std::unique_ptr<llvm::Module> TheModule);
  llvm::Error removeModule(std::unique_ptr<llvm::Module> TheModule);
  llvm::Error runCtors() const;
  llvm::Expected<llvm::JITTargetAddress>
  getSymbolAddress(llvm::StringRef Name) const;
  llvm::Error AddStaticLib(llvm::StringRef Path);
  llvm::Error AddDynamicLib(llvm::StringRef Path);
};

} // end namespace clang

#endif // LLVM_CLANG_TOOLS_CLANG_CCINT_CCINT_JIT_H
