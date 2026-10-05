source_filename = "spillweight"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @spillweight.hot(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca i64, align 8
  %t11 = alloca i64, align 8
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca i8, align 1
  %t15 = alloca i64, align 8
  %t16 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = add i64 %v0, 1
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t0, align 8
  %v3 = add i64 %v2, 2
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t0, align 8
  %v5 = add i64 %v4, 3
  store i64 %v5, ptr %t3, align 8
  %v6 = load i64, ptr %t0, align 8
  %v7 = add i64 %v6, 4
  store i64 %v7, ptr %t4, align 8
  %v8 = load i64, ptr %t0, align 8
  %v9 = add i64 %v8, 5
  store i64 %v9, ptr %t5, align 8
  %v10 = load i64, ptr %t0, align 8
  %v11 = add i64 %v10, 6
  store i64 %v11, ptr %t6, align 8
  %v12 = load i64, ptr %t0, align 8
  %v13 = add i64 %v12, 7
  store i64 %v13, ptr %t7, align 8
  %v14 = load i64, ptr %t0, align 8
  %v15 = add i64 %v14, 8
  store i64 %v15, ptr %t8, align 8
  %v16 = load i64, ptr %t0, align 8
  %v17 = add i64 %v16, 9
  store i64 %v17, ptr %t9, align 8
  %v18 = load i64, ptr %t0, align 8
  %v19 = add i64 %v18, 10
  store i64 %v19, ptr %t10, align 8
  %v20 = load i64, ptr %t0, align 8
  %v21 = add i64 %v20, 11
  store i64 %v21, ptr %t11, align 8
  store i64 0, ptr %t12, align 8
  store i64 0, ptr %t13, align 8
  br label %b1

b1:
  %v22 = load i64, ptr %t13, align 8
  %v23 = load i64, ptr %t0, align 8
  %v24 = icmp slt i64 %v22, %v23
  %v25 = zext i1 %v24 to i8
  store i8 %v25, ptr %t14, align 1
  %v26 = load i8, ptr %t14, align 1
  %v27 = trunc i8 %v26 to i1
  br i1 %v27, label %b2, label %b3

b2:
  %v28 = load i64, ptr %t12, align 8
  %v29 = load i64, ptr %t13, align 8
  %v30 = add i64 %v28, %v29
  store i64 %v30, ptr %t12, align 8
  %v31 = load i64, ptr %t13, align 8
  %v32 = add i64 %v31, 1
  store i64 %v32, ptr %t13, align 8
  br label %b1

b3:
  %v33 = load i64, ptr %t1, align 8
  %v34 = load i64, ptr %t2, align 8
  %v35 = load i64, ptr %t3, align 8
  %v36 = load i64, ptr %t4, align 8
  %v37 = load i64, ptr %t5, align 8
  %v38 = load i64, ptr %t6, align 8
  %v39 = load i64, ptr %t7, align 8
  %v40 = load i64, ptr %t8, align 8
  %v41 = load i64, ptr %t9, align 8
  %v42 = load i64, ptr %t10, align 8
  %v43 = load i64, ptr %t11, align 8
  %v44 = load i64, ptr %t13, align 8
  %v45 = call i64 (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64) @use12(i64 %v33, i64 %v34, i64 %v35, i64 %v36, i64 %v37, i64 %v38, i64 %v39, i64 %v40, i64 %v41, i64 %v42, i64 %v43, i64 %v44)
  store i64 %v45, ptr %t15, align 8
  %v46 = load i64, ptr %t12, align 8
  %v47 = load i64, ptr %t15, align 8
  %v48 = add i64 %v46, %v47
  store i64 %v48, ptr %t16, align 8
  %v49 = load i64, ptr %t16, align 8
  ret i64 %v49
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i64 @use12(i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef, i64 noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
