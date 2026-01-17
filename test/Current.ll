; RUN:  opt -load-pass-plugin %shlibdir/libTailRecursionElimination%shlibext \
; RUN:    -passes="tailrecelim" -debug-only=tailrecelim -stats -disable-output %s 2>&1 \
; RUN:    | FileCheck %s --allow-empty

; CHECK: 0 tailrecelim

; Function Attrs: noinline nounwind optnone ssp uwtable(sync)
define i32 @f(i32 noundef %x) #0 {
entry:
  %retval = alloca i32, align 4
  %x.addr = alloca i32, align 4
  store i32 %x, ptr %x.addr, align 4
  %0 = load i32, ptr %x.addr, align 4
  %cmp = icmp eq i32 %0, 1
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i32, ptr %x.addr, align 4
  store i32 %1, ptr %retval, align 4
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i32, ptr %x.addr, align 4
  %sub = sub nsw i32 %2, 1
  %call = call i32 @f(i32 noundef %sub)
  %mul = mul nsw i32 2, %call
  store i32 %mul, ptr %retval, align 4
  br label %return

return:                                           ; preds = %if.end, %if.then
  %3 = load i32, ptr %retval, align 4
  ret i32 %3
}
