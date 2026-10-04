source_filename = "com.example.doubling"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i64 @twice(i64 %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = and i64 1, 63
  %v2 = shl i64 %v0, %v1
  store i64 %v2, ptr %t1, align 8
  %v3 = load i64, ptr %t1, align 8
  ret i64 %v3
}

@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @anti_rt_init, ptr null }]

declare void @anti_rt_init() #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
