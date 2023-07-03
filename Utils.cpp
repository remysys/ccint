#include "Utils.h"

#include "clang/AST/GlobalDecl.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/BinaryFormat/Magic.h"

namespace clang {

class PPLexer : public Lexer {
public:
  PPLexer(const LangOptions &LangOpts, llvm::StringRef Code)
      : Lexer(SourceLocation(), LangOpts, Code.begin(), Code.begin(),
              Code.end()) {}

  bool inPPDirective() const { return ParsingPreprocessorDirective; }

  bool AdvanceTo(clang::Token &Tok, clang::tok::TokenKind kind) {
    while (!Lex(Tok)) {
      if (Tok.is(kind))
        return false;
    }
    return true;
  }

  bool Lex(clang::Token &Tok) {
    bool ret = LexFromRawLexer(Tok);
    if (inPPDirective()) {
      if (Tok.is(tok::eod))
        ParsingPreprocessorDirective = false;
    } else {
      if (Tok.is(tok::hash)) {
        ParsingPreprocessorDirective = true;
      }
    }
    return ret;
  }
};

size_t getFileOffset(const clang::Token &Tok) {
  return Tok.getLocation().getRawEncoding();
}

size_t getWrapPos(const clang::LangOptions &LangOpts, const std::string &Code) {

  PPLexer Lex(LangOpts, Code);
  Token token;

  while (true) {
    bool atEOF = Lex.Lex(token);
    if (Lex.inPPDirective() || token.is(tok::eod)) {
      if (atEOF)
        break;
      continue;
    }

    if (token.is(tok::eof)) {
      return std::string::npos;
    }

    const tok::TokenKind kind = token.getKind();

    if (kind == tok::raw_identifier) {
      StringRef keyword(token.getRawIdentifier());
      if (keyword.equals("using")) {
        if (Lex.AdvanceTo(token, tok::semi)) {
          return std::string::npos;
        }
        return getFileOffset(token) + 1;
      }
    }

    return getFileOffset(token);
  }

  return std::string::npos;
}

bool isCCIntMain(clang::FunctionDecl *FD) {
  if (!FD) {
    return false;
  }

  if (!FD->getDeclName().isIdentifier()) {
    return false;
  }
  return FD->getName().startswith("ccint_main");
}

bool isDynamicLibrary(llvm::StringRef Path) {
  llvm::file_magic type;

  std::error_code EC = identify_magic(Path, type);
  if (EC) {
    return false;
  }

  switch (type) {
  default:
    return false;
  case llvm::file_magic::macho_fixed_virtual_memory_shared_lib:
  case llvm::file_magic::macho_dynamically_linked_shared_lib:
  case llvm::file_magic::macho_dynamically_linked_shared_lib_stub:
  case llvm::file_magic::elf_shared_object:
  case llvm::file_magic::pecoff_executable:
    return true;
  }
}

} // namespace clang