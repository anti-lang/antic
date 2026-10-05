source_filename = "sum"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @sum.sum(ptr noundef nonnull dereferenceable(4) %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i8, align 1
  %t5 = alloca i64, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i32, align 4
  %t8 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store i64 0, ptr %t2, align 8
  store i64 0, ptr %t3, align 8
  br label %b1

b1:
  %v0 = load i64, ptr %t3, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = icmp slt i64 %v0, %v1
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t4, align 1
  %v4 = load i8, ptr %t4, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b2, label %b3

b2:
  %v6 = load i64, ptr %t3, align 8
  %v7 = and i64 2, 63
  %v8 = shl i64 %v6, %v7
  store i64 %v8, ptr %t5, align 8
  %v9 = load ptr, ptr %t0, align 8
  %v10 = load i64, ptr %t5, align 8
  %v11 = getelementptr i8, ptr %v9, i64 %v10
  store ptr %v11, ptr %t6, align 8
  %v12 = load ptr, ptr %t6, align 8
  %v13 = load i32, ptr %v12, align 4
  store i32 %v13, ptr %t7, align 4
  %v14 = load i32, ptr %t7, align 4
  %v15 = sext i32 %v14 to i64
  store i64 %v15, ptr %t8, align 8
  %v16 = load i64, ptr %t2, align 8
  %v17 = load i64, ptr %t8, align 8
  %v18 = add i64 %v16, %v17
  store i64 %v18, ptr %t2, align 8
  %v19 = load i64, ptr %t3, align 8
  %v20 = add i64 %v19, 1
  store i64 %v20, ptr %t3, align 8
  br label %b1

b3:
  %v21 = load i64, ptr %t2, align 8
  ret i64 %v21
}

define internal void @sum.fill(ptr noundef nonnull dereferenceable(2) %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i8, align 1
  %t4 = alloca i64, align 8
  %t5 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store i64 0, ptr %t2, align 8
  br label %b1

b1:
  %v0 = load i64, ptr %t2, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = icmp slt i64 %v0, %v1
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t3, align 1
  %v4 = load i8, ptr %t3, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b2, label %b3

b2:
  %v6 = load i64, ptr %t2, align 8
  %v7 = and i64 1, 63
  %v8 = shl i64 %v6, %v7
  store i64 %v8, ptr %t4, align 8
  %v9 = load ptr, ptr %t0, align 8
  %v10 = load i64, ptr %t4, align 8
  %v11 = getelementptr i8, ptr %v9, i64 %v10
  store ptr %v11, ptr %t5, align 8
  %v12 = load ptr, ptr %t5, align 8
  store i16 7, ptr %v12, align 2
  %v13 = load i64, ptr %t2, align 8
  %v14 = add i64 %v13, 1
  store i64 %v14, ptr %t2, align 8
  br label %b1

b3:
  ret void
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
