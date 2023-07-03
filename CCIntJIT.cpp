#include "CCIntJIT.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"

namespace clang {

CCIntJIT::CCIntJIT(llvm::orc::ThreadSafeContext &TSC, llvm::Error &Err,
                   const clang::TargetInfo &TI)
    : TSCtx(TSC) {

  using namespace llvm::orc;
  llvm::ErrorAsOutParameter EAO(&Err);

  auto JTMB = JITTargetMachineBuilder(TI.getTriple());
  JTMB.addFeatures(TI.getTargetOpts().Features);
  if (auto JitOrErr = LLJITBuilder().setJITTargetMachineBuilder(JTMB).create())
    Jit = std::move(*JitOrErr);
  else {
    Err = JitOrErr.takeError();
    return;
  }

  if (auto GeneratorOrErr = DynamicLibrarySearchGenerator::GetForCurrentProcess(
          Jit->getDataLayout().getGlobalPrefix())) {
    Jit->getMainJITDylib().addGenerator(std::move(*GeneratorOrErr));
  } else {
    Err = GeneratorOrErr.takeError();
    return;
  }
}

CCIntJIT::~CCIntJIT() {}

llvm::Error CCIntJIT::addModule(std::unique_ptr<llvm::Module> TheModule) {
  llvm::orc::ResourceTrackerSP RT =
      Jit->getMainJITDylib().createResourceTracker();
  ResourceTrackers[TheModule.get()] = RT;

  return Jit->addIRModule(RT, {std::move(TheModule), TSCtx});
}

llvm::Error CCIntJIT::removeModule(std::unique_ptr<llvm::Module> TheModule) {

  llvm::orc::ResourceTrackerSP RT =
      std::move(ResourceTrackers[TheModule.get()]);
  if (!RT)
    return llvm::Error::success();

  ResourceTrackers.erase(TheModule.get());
  if (llvm::Error Err = RT->remove())
    return Err;
  return llvm::Error::success();
}

llvm::Error CCIntJIT::runCtors() const {
  return Jit->initialize(Jit->getMainJITDylib());
}

llvm::Expected<llvm::JITTargetAddress>
CCIntJIT::getSymbolAddress(llvm::StringRef Name) const {
  auto Sym = Jit->lookup(Name);

  if (!Sym)
    return Sym.takeError();
  return Sym->getValue();
}

llvm::Error CCIntJIT::AddStaticLib(llvm::StringRef Path) {
  auto G = llvm::orc::StaticLibraryDefinitionGenerator::Load(
      Jit->getObjLinkingLayer(), Path.data());
  if (!G)
    return G.takeError();

  Jit->getMainJITDylib().addGenerator(std::move(*G));

  return llvm::Error::success();
}

llvm::Error CCIntJIT::AddDynamicLib(llvm::StringRef Path) {
  std::string ErrMsg;
  if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(Path.data(), &ErrMsg)) {
    return llvm::createStringError(llvm::inconvertibleErrorCode(), ErrMsg);
  }

  return llvm::Error::success();
}

} // end namespace clang
