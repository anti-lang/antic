source_filename = "control_flow"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define hidden i64 @control_flow.choose(i8 zeroext %p0, i64 %p1, i64 %p2) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  store i8 %p0, ptr %t0, align 1
  store i64 %p1, ptr %t1, align 8
  store i64 %p2, ptr %t2, align 8
  %v0 = load i64, ptr %t1, align 8
  store i64 %v0, ptr %t3, align 8
  %v1 = load i8, ptr %t0, align 1
  %v2 = trunc i8 %v1 to i1
  br i1 %v2, label %b1, label %b2

b1:
  %v3 = load i64, ptr %t2, align 8
  store i64 %v3, ptr %t3, align 8
  br label %b2

b2:
  %v4 = load i64, ptr %t3, align 8
  ret i64 %v4
}

define hidden i8 @control_flow.spin(i8 zeroext %p0, i8 zeroext %p1) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i8, align 1
  store i8 %p0, ptr %t0, align 1
  store i8 %p1, ptr %t1, align 1
  br label %b1

b1:
  %v0 = load i8, ptr %t0, align 1
  %v1 = trunc i8 %v0 to i1
  br i1 %v1, label %b2, label %b3

b2:
  %v2 = load i8, ptr %t1, align 1
  %v3 = trunc i8 %v2 to i1
  br i1 %v3, label %b4, label %b1

b3:
  %v4 = load i8, ptr %t1, align 1
  ret i8 %v4

b4:
  %v5 = load i8, ptr %t0, align 1
  ret i8 %v5
}

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
