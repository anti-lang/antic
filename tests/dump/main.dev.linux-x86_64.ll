source_filename = "main"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define hidden i64 @main.main() #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %v0 = call i64 (i64) @com.example.twice.twice(i64 7)
  store i64 %v0, ptr %t0, align 8
  %v1 = load i64, ptr %t0, align 8
  %v2 = icmp eq i64 2, 0
  %v3 = icmp eq i64 2, -1
  %v4 = or i1 %v2, %v3
  %v5 = select i1 %v4, i64 1, i64 2
  %v6 = sdiv i64 %v1, %v5
  %v7 = sub i64 0, %v1
  %v8 = select i1 %v3, i64 %v7, i64 %v6
  %v9 = select i1 %v2, i64 0, i64 %v8
  store i64 %v9, ptr %t1, align 8
  %v10 = load i64, ptr %t1, align 8
  ret i64 %v10
}

@anti.rt.main = alias i64 (), ptr @main.main

@main.0 = hidden constant <{ [35 x i8] }> <{ [35 x i8] c"main.anti:5: division by zero in /\00" }>, align 1
@anti_rt_registry = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8
@anti_rt_backtrace_default = dso_local constant <{ [8 x i8] }> <{ [8 x i8] c"\01\00\00\00\00\00\00\00" }>, align 8
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8
@anti_rt_trampolines = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare i64 @com.example.twice.twice(i64) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
