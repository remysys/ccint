#include "CCIntParser.h"
#include "Utils.h"

#include "clang/AST/DeclContextInternals.h"
#include "clang/CodeGen/BackendUtil.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/FrontendTool/Utils.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Parse/Parser.h"
#include "clang/Sema/Sema.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Timer.h"

#include <iostream>
#include <memory>
#include <regex>

namespace clang {

class CCIntAction : public WrapperFrontendAction {
private:
  std::string MangledName;

public:
  CCIntAction(CompilerInstance &CI, llvm::LLVMContext &LLVMCtx,
              llvm::Error &Err)
      : WrapperFrontendAction([&]() {
          llvm::ErrorAsOutParameter EAO(&Err);

          std::unique_ptr<FrontendAction> Act;
          Act.reset(new EmitLLVMOnlyAction(&LLVMCtx));
          return Act;
        }()) {}

  FrontendAction *getWrapped() const { return WrappedAction.get(); }
  llvm::StringRef GetMangledName() const { return MangledName; };

  void ExecuteAction() override {
    WrapperFrontendAction::ExecuteAction();
    TranslationUnitDecl *TUDecl =
        getCompilerInstance().getASTContext().getTranslationUnitDecl();

    for (Decl *D : TUDecl->decls()) {
      if (FunctionDecl *FD = llvm::dyn_cast<FunctionDecl>(D)) {
        if (isCCIntMain(FD)) {
          CodeGenerator *CG =
              static_cast<CodeGenAction *>(getWrapped())->getCodeGenerator();
          assert(CG);
          MangledName = CG->GetMangledName(FD).str();
        }
      }
    }
  }
};

CCIntParser::CCIntParser(std::unique_ptr<CompilerInstance> Instance,
                         llvm::LLVMContext &LLVMCtx, llvm::Error &Err)
    : CI(std::move(Instance)) {

  Act = std::make_unique<CCIntAction>(*CI, LLVMCtx, Err);
}

CCIntParser::~CCIntParser() {}

llvm::Error CCIntParser::Parse(llvm::StringRef FileName, bool Wrap) {
  if (!Act) {
    return llvm::createStringError(llvm::errc::not_supported, "parse failed");
  }

  FrontendInputFile InputFile(FileName,
                              CI->getFrontendOpts().Inputs[0].getKind());

  CI->getInvocation().getFrontendOpts().Inputs.clear();
  CI->getInvocation().getFrontendOpts().Inputs.push_back(InputFile);

  std::string wrapCode;
  if (Wrap) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> MBOrErr =
        llvm::MemoryBuffer::getFile(FileName);
    if (std::error_code error = MBOrErr.getError()) {
      return llvm::createStringError(llvm::errc::not_supported,
                                     "failed to read file: %s",
                                     error.message().c_str());
    }

    llvm::StringRef Code(MBOrErr.get()->getBufferStart(),
                         MBOrErr.get()->getBufferSize());

    wrapCode = WrapInput(Code.str());

    std::unique_ptr<llvm::MemoryBuffer> MB =
        llvm::MemoryBuffer::getMemBuffer(wrapCode);
    CI->getPreprocessorOpts().addRemappedFile(FileName, MB.release());
  }

  bool Success = CI->ExecuteAction(*Act);
  if (!Success) {
    return llvm::createStringError(llvm::errc::not_supported, "parse failed");
  }

  TheModule = static_cast<CodeGenAction *>(Act->getWrapped())->takeModule();

  return llvm::Error::success();
}

llvm::StringRef CCIntParser::GetMangledName() const {
  return Act->GetMangledName();
}

std::string CCIntParser::WrapInput(const std::string &Code) {

  size_t wrapPos = getWrapPos(CI->getLangOpts(), Code);

  if (wrapPos != std::string::npos) {
    std::string res = Code.substr(0, wrapPos) + "void ccint_main() {\n" +
                      Code.substr(wrapPos) + "\n}";

    return res;
  }

  return Code;
}

} // end namespace clang
