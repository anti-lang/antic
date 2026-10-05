source_filename = "spill7"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @spill7.seven(i64 noundef %p0) #0 {
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
  %t14 = alloca i64, align 8
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
  %v14 = call i64 () @tick()
  store i64 %v14, ptr %t8, align 8
  %v15 = load i64, ptr %t1, align 8
  %v16 = load i64, ptr %t2, align 8
  %v17 = add i64 %v15, %v16
  store i64 %v17, ptr %t9, align 8
  %v18 = load i64, ptr %t9, align 8
  %v19 = load i64, ptr %t3, align 8
  %v20 = add i64 %v18, %v19
  store i64 %v20, ptr %t10, align 8
  %v21 = load i64, ptr %t10, align 8
  %v22 = load i64, ptr %t4, align 8
  %v23 = add i64 %v21, %v22
  store i64 %v23, ptr %t11, align 8
  %v24 = load i64, ptr %t11, align 8
  %v25 = load i64, ptr %t5, align 8
  %v26 = add i64 %v24, %v25
  store i64 %v26, ptr %t12, align 8
  %v27 = load i64, ptr %t12, align 8
  %v28 = load i64, ptr %t6, align 8
  %v29 = add i64 %v27, %v28
  store i64 %v29, ptr %t13, align 8
  %v30 = load i64, ptr %t13, align 8
  %v31 = load i64, ptr %t7, align 8
  %v32 = add i64 %v30, %v31
  store i64 %v32, ptr %t14, align 8
  %v33 = load i64, ptr %t14, align 8
  ret i64 %v33
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i64 @tick() #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
