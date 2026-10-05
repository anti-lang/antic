source_filename = "spill12"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i64 @spill12.many(i64 noundef %p0) #0 {
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
  %t15 = alloca i64, align 8
  %t16 = alloca i64, align 8
  %t17 = alloca i64, align 8
  %t18 = alloca i64, align 8
  %t19 = alloca i64, align 8
  %t20 = alloca i64, align 8
  %t21 = alloca i64, align 8
  %t22 = alloca i64, align 8
  %t23 = alloca i64, align 8
  %t24 = alloca i64, align 8
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
  %v22 = load i64, ptr %t0, align 8
  %v23 = add i64 %v22, 12
  store i64 %v23, ptr %t12, align 8
  %v24 = call i64 () @tick()
  store i64 %v24, ptr %t13, align 8
  %v25 = load i64, ptr %t1, align 8
  %v26 = load i64, ptr %t2, align 8
  %v27 = add i64 %v25, %v26
  store i64 %v27, ptr %t14, align 8
  %v28 = load i64, ptr %t14, align 8
  %v29 = load i64, ptr %t3, align 8
  %v30 = add i64 %v28, %v29
  store i64 %v30, ptr %t15, align 8
  %v31 = load i64, ptr %t15, align 8
  %v32 = load i64, ptr %t4, align 8
  %v33 = add i64 %v31, %v32
  store i64 %v33, ptr %t16, align 8
  %v34 = load i64, ptr %t16, align 8
  %v35 = load i64, ptr %t5, align 8
  %v36 = add i64 %v34, %v35
  store i64 %v36, ptr %t17, align 8
  %v37 = load i64, ptr %t17, align 8
  %v38 = load i64, ptr %t6, align 8
  %v39 = add i64 %v37, %v38
  store i64 %v39, ptr %t18, align 8
  %v40 = load i64, ptr %t18, align 8
  %v41 = load i64, ptr %t7, align 8
  %v42 = add i64 %v40, %v41
  store i64 %v42, ptr %t19, align 8
  %v43 = load i64, ptr %t19, align 8
  %v44 = load i64, ptr %t8, align 8
  %v45 = add i64 %v43, %v44
  store i64 %v45, ptr %t20, align 8
  %v46 = load i64, ptr %t20, align 8
  %v47 = load i64, ptr %t9, align 8
  %v48 = add i64 %v46, %v47
  store i64 %v48, ptr %t21, align 8
  %v49 = load i64, ptr %t21, align 8
  %v50 = load i64, ptr %t10, align 8
  %v51 = add i64 %v49, %v50
  store i64 %v51, ptr %t22, align 8
  %v52 = load i64, ptr %t22, align 8
  %v53 = load i64, ptr %t11, align 8
  %v54 = add i64 %v52, %v53
  store i64 %v54, ptr %t23, align 8
  %v55 = load i64, ptr %t23, align 8
  %v56 = load i64, ptr %t12, align 8
  %v57 = add i64 %v55, %v56
  store i64 %v57, ptr %t24, align 8
  %v58 = load i64, ptr %t24, align 8
  ret i64 %v58
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i64 @tick() #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
