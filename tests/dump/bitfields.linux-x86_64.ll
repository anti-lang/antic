source_filename = "bitfields"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal i64 @bitfields.main() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i32, align 4
  %t2 = alloca i32, align 4
  %t3 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t5 = alloca i32, align 4
  %t6 = alloca i64, align 8
  %t7 = alloca i32, align 4
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca i64, align 8
  %t11 = alloca i8, align 1
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca i64, align 8
  %t15 = alloca i16, align 2
  %t16 = alloca i64, align 8
  %t17 = alloca i64, align 8
  %s0 = alloca [4 x i8], align 4
  store ptr %s0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i8, ptr %v0, align 1
  %v2 = trunc i32 1 to i8
  %v3 = and i8 %v2, 1
  %v4 = and i8 %v1, -2
  %v5 = or i8 %v4, %v3
  store i8 %v5, ptr %v0, align 1
  %v6 = load ptr, ptr %t0, align 8
  %v7 = load i8, ptr %v6, align 1
  %v8 = trunc i32 9 to i8
  %v9 = and i8 %v8, 15
  %v10 = shl i8 %v9, 1
  %v11 = and i8 %v7, -31
  %v12 = or i8 %v11, %v10
  store i8 %v12, ptr %v6, align 1
  %v13 = load ptr, ptr %t0, align 8
  %v14 = load i8, ptr %v13, align 1
  %v15 = and i8 -3, 7
  %v16 = shl i8 %v15, 5
  %v17 = and i8 %v14, 31
  %v18 = or i8 %v17, %v16
  store i8 %v18, ptr %v13, align 1
  %v19 = load ptr, ptr %t0, align 8
  %v20 = getelementptr i8, ptr %v19, i64 2
  %v21 = load i16, ptr %v20, align 2
  %v22 = and i16 300, 511
  %v23 = and i16 %v21, -512
  %v24 = or i16 %v23, %v22
  store i16 %v24, ptr %v20, align 2
  %v25 = load ptr, ptr %t0, align 8
  %v26 = load i8, ptr %v25, align 1
  %v27 = lshr i8 %v26, 1
  %v28 = and i8 %v27, 15
  %v29 = zext i8 %v28 to i32
  store i32 %v29, ptr %t1, align 4
  %v30 = load i32, ptr %t1, align 4
  %v31 = add i32 %v30, 3
  store i32 %v31, ptr %t2, align 4
  %v32 = load i32, ptr %t2, align 4
  %v33 = load ptr, ptr %t0, align 8
  %v34 = load i8, ptr %v33, align 1
  %v35 = trunc i32 %v32 to i8
  %v36 = and i8 %v35, 15
  %v37 = shl i8 %v36, 1
  %v38 = and i8 %v34, -31
  %v39 = or i8 %v38, %v37
  store i8 %v39, ptr %v33, align 1
  %v40 = load ptr, ptr %t0, align 8
  %v41 = load i8, ptr %v40, align 1
  %v42 = ashr i8 %v41, 5
  store i8 %v42, ptr %t3, align 1
  %v43 = load i8, ptr %t3, align 1
  %v44 = sub i8 %v43, 1
  store i8 %v44, ptr %t4, align 1
  %v45 = load i8, ptr %t4, align 1
  %v46 = load ptr, ptr %t0, align 8
  %v47 = load i8, ptr %v46, align 1
  %v48 = and i8 %v45, 7
  %v49 = shl i8 %v48, 5
  %v50 = and i8 %v47, 31
  %v51 = or i8 %v50, %v49
  store i8 %v51, ptr %v46, align 1
  %v52 = load ptr, ptr %t0, align 8
  %v53 = load i8, ptr %v52, align 1
  %v54 = and i8 %v53, 1
  %v55 = zext i8 %v54 to i32
  store i32 %v55, ptr %t5, align 4
  %v56 = load i32, ptr %t5, align 4
  %v57 = zext i32 %v56 to i64
  store i64 %v57, ptr %t6, align 8
  %v58 = load ptr, ptr %t0, align 8
  %v59 = load i8, ptr %v58, align 1
  %v60 = lshr i8 %v59, 1
  %v61 = and i8 %v60, 15
  %v62 = zext i8 %v61 to i32
  store i32 %v62, ptr %t7, align 4
  %v63 = load i32, ptr %t7, align 4
  %v64 = zext i32 %v63 to i64
  store i64 %v64, ptr %t8, align 8
  %v65 = load i64, ptr %t8, align 8
  %v66 = mul i64 %v65, 10
  store i64 %v66, ptr %t9, align 8
  %v67 = load i64, ptr %t6, align 8
  %v68 = load i64, ptr %t9, align 8
  %v69 = add i64 %v67, %v68
  store i64 %v69, ptr %t10, align 8
  %v70 = load ptr, ptr %t0, align 8
  %v71 = load i8, ptr %v70, align 1
  %v72 = ashr i8 %v71, 5
  store i8 %v72, ptr %t11, align 1
  %v73 = load i8, ptr %t11, align 1
  %v74 = sext i8 %v73 to i64
  store i64 %v74, ptr %t12, align 8
  %v75 = load i64, ptr %t12, align 8
  %v76 = mul i64 %v75, 100
  store i64 %v76, ptr %t13, align 8
  %v77 = load i64, ptr %t10, align 8
  %v78 = load i64, ptr %t13, align 8
  %v79 = add i64 %v77, %v78
  store i64 %v79, ptr %t14, align 8
  %v80 = load ptr, ptr %t0, align 8
  %v81 = getelementptr i8, ptr %v80, i64 2
  %v82 = load i16, ptr %v81, align 2
  %v83 = and i16 %v82, 511
  store i16 %v83, ptr %t15, align 2
  %v84 = load i16, ptr %t15, align 2
  %v85 = zext i16 %v84 to i64
  store i64 %v85, ptr %t16, align 8
  %v86 = load i64, ptr %t14, align 8
  %v87 = load i64, ptr %t16, align 8
  %v88 = add i64 %v86, %v87
  store i64 %v88, ptr %t17, align 8
  %v89 = load i64, ptr %t17, align 8
  ret i64 %v89
}

@anti.rt.main = alias i64 (), ptr @bitfields.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
