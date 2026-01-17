; RUN:  opt -load-pass-plugin %shlibdir/libTailRecursionElimination%shlibext \
; RUN:    -passes="tailrecelim" -debug-only=tailrecelim -stats -disable-output %s 2>&1 | FileCheck %s --allow-empty

; CHECK: 0 tailrecelim

define i32 @foo(i32) {
  ret i32 %0
}

define i32 @bar(i32) {
  ret i32 %0
}
