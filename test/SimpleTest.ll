; This file is used by `apple-silicon.yml`, `apple-x86.yml`, and `ubuntu-x86.yml` workflow.
; Since the LLVM build does not include debug information, we cannot use `-debug-only` and `-stats` flags.

; RUN:  opt -load-pass-plugin %shlibdir/libTailRecursionElimination%shlibext \
; RUN:    -passes="tailrecelim" -disable-output %s 2>&1 | FileCheck %s --allow-empty

; CHECK-NOT: empty

define i32 @foo(i32) {
  ret i32 %0
}

define i32 @bar(i32) {
  ret i32 %0
}
