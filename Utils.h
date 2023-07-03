#ifndef LLVM_CLANG_TOOLS_CLANG_CCINT_UTILS_H
#define LLVM_CLANG_TOOLS_CLANG_CCINT_UTILS_H

#include "llvm/ADT/StringRef.h"

#include <string>

namespace clang {
class LangOptions;
class FunctionDecl;

size_t getWrapPos(const clang::LangOptions &LangOpts, const std::string &Code);
bool isCCIntMain(clang::FunctionDecl *FD);

bool isDynamicLibrary(llvm::StringRef Path);

} // namespace clang

#endif // LLVM_CLANG_TOOLS_CLANG_CCINT_UTILS_H