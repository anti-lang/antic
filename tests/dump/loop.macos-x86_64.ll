source_filename = "loop"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macos11.0"

define internal i64 @loop.g(i64 %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca i8, align 1
  store i64 %p0, ptr %t0, align 8
  store i64 0, ptr %t1, align 8
  br label %b1

b1:
  %v0 = load i64, ptr %t1, align 8
  %v1 = load i64, ptr %t0, align 8
  %v2 = icmp slt i64 %v0, %v1
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t2, align 1
  %v4 = load i8, ptr %t2, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b2, label %b3

b2:
  %v6 = load i64, ptr %t1, align 8
  %v7 = add i64 %v6, 1
  store i64 %v7, ptr %t1, align 8
  br label %b1

b3:
  %v8 = load i64, ptr %t1, align 8
  %v9 = icmp sgt i64 %v8, 3
  %v10 = zext i1 %v9 to i8
  store i8 %v10, ptr %t3, align 1
  %v11 = load i8, ptr %t3, align 1
  %v12 = trunc i8 %v11 to i1
  br i1 %v12, label %b4, label %b5

b4:
  ret i64 1

b5:
  ret i64 0
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
