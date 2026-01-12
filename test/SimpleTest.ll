; RUN:  opt -load-pass-plugin %shlibdir/libTailRecursionElimination%shlibext -passes="tailrecelim" -disable-output %s 2>&1 | FileCheck %s


; Makes sure that the functions are counted correctly.

; CHECK-NOT: foo
; CHECK-NOT: bar

; CHECK:      Unimplemented
; CHECK-NEXT: Unimplemented

define i32 @foo(i32) {
  ret i32 %0
}

define i32 @bar(i32) {
  ret i32 %0
}
