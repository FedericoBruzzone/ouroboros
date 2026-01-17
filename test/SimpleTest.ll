; RUN:  opt -load-pass-plugin %shlibdir/libTailRecursionElimination%shlibext \
; RUN:    -passes="tailrecelim" -stats -disable-output %s 2>&1 | FileCheck %s --allow-empty

; In general, it is not recommended to use `-debug-only` flag in production builds.
; For CI/CD purposes, since the LLVM build does not include debug information, we cannot use `-debug-only` flag.

; CHECK: 0 tailrecelim

define i32 @foo(i32) {
  ret i32 %0
}

define i32 @bar(i32) {
  ret i32 %0
}
