source_filename = "sizes"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @sizes.main() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca ptr, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca i8, align 1
  %t7 = alloca ptr, align 8
  %t8 = alloca i8, align 1
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  %t11 = alloca i32, align 4
  %s0 = alloca [16 x i8], align 1
  store ptr %s0, ptr %t0, align 8
  store i64 0, ptr %t1, align 8
  br label %b1

b1:
  %v0 = load i64, ptr %t1, align 8
  %v1 = icmp slt i64 %v0, 16
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t2, align 1
  %v3 = load i8, ptr %t2, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b2, label %b3

b2:
  %v5 = load ptr, ptr %t0, align 8
  %v6 = load i64, ptr %t1, align 8
  %v7 = getelementptr i8, ptr %v5, i64 %v6
  store ptr %v7, ptr %t3, align 8
  %v8 = load ptr, ptr %t3, align 8
  store i8 1, ptr %v8, align 1
  %v9 = load i64, ptr %t1, align 8
  %v10 = add i64 %v9, 1
  store i64 %v10, ptr %t1, align 8
  br label %b1

b3:
  store i64 0, ptr %t4, align 8
  store i64 0, ptr %t5, align 8
  br label %b4

b4:
  %v11 = load i64, ptr %t5, align 8
  %v12 = icmp slt i64 %v11, 16
  %v13 = zext i1 %v12 to i8
  store i8 %v13, ptr %t6, align 1
  %v14 = load i8, ptr %t6, align 1
  %v15 = trunc i8 %v14 to i1
  br i1 %v15, label %b5, label %b6

b5:
  %v16 = load ptr, ptr %t0, align 8
  %v17 = load i64, ptr %t5, align 8
  %v18 = getelementptr i8, ptr %v16, i64 %v17
  store ptr %v18, ptr %t7, align 8
  %v19 = load ptr, ptr %t7, align 8
  %v20 = load i8, ptr %v19, align 1
  store i8 %v20, ptr %t8, align 1
  %v21 = load i8, ptr %t8, align 1
  %v22 = zext i8 %v21 to i64
  store i64 %v22, ptr %t9, align 8
  %v23 = load i64, ptr %t4, align 8
  %v24 = load i64, ptr %t9, align 8
  %v25 = add i64 %v23, %v24
  store i64 %v25, ptr %t4, align 8
  %v26 = load i64, ptr %t5, align 8
  %v27 = add i64 %v26, 1
  store i64 %v27, ptr %t5, align 8
  br label %b4

b6:
  store ptr @sizes.15, ptr %t10, align 8
  %v28 = load ptr, ptr %t10, align 8
  %v29 = load i64, ptr %t4, align 8
  %v30 = call i32 (ptr, ...) @printf(ptr %v28, i64 16, i64 %v29, i64 24)
  store i32 %v30, ptr %t11, align 4
  ret i64 0
}

@anti.rt.main = alias i64 (), ptr @sizes.main

@sizes.15 = internal constant <{ [16 x i8] }> <{ [16 x i8] c"%lld %lld %lld\0A\00" }>, align 1
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @printf(ptr noundef, ...) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
