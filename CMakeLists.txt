set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Core
  LineEditor
  Option
  OrcJIT
  Support
  native
  Target
  )


add_clang_tool(clang-ccint
  Driver.cpp
  CCIntJIT.cpp
  Interpreter.cpp
  CCIntParser.cpp
  Utils.cpp
  )

clang_target_link_libraries(clang-ccint PRIVATE
  clangBasic
  clangFrontend
  clangTooling
  clangAST
  clangAnalysis
  clangDriver
  clangEdit
  clangLex
  clangParse
  clangSema
  clangSerialization
  clangCodeGen
  clangFrontendTool
  )