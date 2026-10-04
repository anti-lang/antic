source_filename = "floats"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal double @floats.keep(ptr %p0, double %p1, float %p2) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca double, align 8
  %t2 = alloca float, align 4
  %t3 = alloca float, align 4
  %t4 = alloca float, align 4
  %t5 = alloca float, align 4
  %t6 = alloca i8, align 1
  %t7 = alloca double, align 8
  %t8 = alloca double, align 8
  %t9 = alloca double, align 8
  %t10 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  store double %p1, ptr %t1, align 8
  store float %p2, ptr %t2, align 4
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load float, ptr %v0, align 4
  store float %v1, ptr %t3, align 4
  %v2 = load float, ptr %t3, align 4
  %v3 = fmul float %v2, 0x3FE0000000000000
  store float %v3, ptr %t4, align 4
  %v4 = load float, ptr %t4, align 4
  %v5 = load float, ptr %t2, align 4
  %v6 = fsub float %v4, %v5
  store float %v6, ptr %t5, align 4
  %v7 = load float, ptr %t5, align 4
  %v8 = load ptr, ptr %t0, align 8
  store float %v7, ptr %v8, align 4
  %v9 = load double, ptr %t1, align 8
  %v10 = fcmp olt double %v9, 0x3FF8000000000000
  %v11 = zext i1 %v10 to i8
  store i8 %v11, ptr %t6, align 1
  %v12 = load i8, ptr %t6, align 1
  %v13 = trunc i8 %v12 to i1
  br i1 %v13, label %b1, label %b3

b1:
  %v14 = load double, ptr %t1, align 8
  %v15 = fneg double %v14
  store double %v15, ptr %t7, align 8
  %v16 = load double, ptr %t7, align 8
  ret double %v16

b2:
  %v17 = load float, ptr %t2, align 4
  %v18 = fpext float %v17 to double
  store double %v18, ptr %t8, align 8
  %v19 = load double, ptr %t1, align 8
  %v20 = load double, ptr %t8, align 8
  %v21 = fdiv double %v19, %v20
  store double %v21, ptr %t9, align 8
  %v22 = load double, ptr %t9, align 8
  ret double %v22

b3:
  %v23 = load double, ptr %t1, align 8
  %v24 = load double, ptr %t1, align 8
  %v25 = fcmp une double %v23, %v24
  %v26 = zext i1 %v25 to i8
  store i8 %v26, ptr %t10, align 1
  %v27 = load i8, ptr %t10, align 1
  %v28 = trunc i8 %v27 to i1
  br i1 %v28, label %b1, label %b2
}

define internal i64 @floats.convert(i8 signext %p0, i64 %p1, double %p2, float %p3) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i64, align 8
  %t2 = alloca double, align 8
  %t3 = alloca float, align 4
  %t4 = alloca float, align 4
  %t5 = alloca float, align 4
  %t6 = alloca double, align 8
  %t7 = alloca double, align 8
  %t8 = alloca double, align 8
  %t9 = alloca double, align 8
  %t10 = alloca i64, align 8
  %t11 = alloca i32, align 4
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca i32, align 4
  %t15 = alloca i64, align 8
  %t16 = alloca i64, align 8
  store i8 %p0, ptr %t0, align 1
  store i64 %p1, ptr %t1, align 8
  store double %p2, ptr %t2, align 8
  store float %p3, ptr %t3, align 4
  %v0 = load i8, ptr %t0, align 1
  %v1 = sitofp i8 %v0 to float
  store float %v1, ptr %t4, align 4
  %v2 = load float, ptr %t4, align 4
  %v3 = load float, ptr %t3, align 4
  %v4 = fadd float %v2, %v3
  store float %v4, ptr %t5, align 4
  %v5 = load i64, ptr %t1, align 8
  %v6 = uitofp i64 %v5 to double
  store double %v6, ptr %t6, align 8
  %v7 = load double, ptr %t6, align 8
  %v8 = load double, ptr %t2, align 8
  %v9 = fmul double %v7, %v8
  store double %v9, ptr %t7, align 8
  %v10 = load float, ptr %t5, align 4
  %v11 = fpext float %v10 to double
  store double %v11, ptr %t8, align 8
  %v12 = load double, ptr %t8, align 8
  %v13 = load double, ptr %t7, align 8
  %v14 = fadd double %v12, %v13
  store double %v14, ptr %t9, align 8
  %v15 = load double, ptr %t9, align 8
  %v16 = call i64 @llvm.fptoui.sat.i64.f64(double %v15)
  store i64 %v16, ptr %t10, align 8
  %v17 = load double, ptr %t2, align 8
  %v18 = call i32 @llvm.fptosi.sat.i32.f64(double %v17)
  store i32 %v18, ptr %t11, align 4
  %v19 = load i32, ptr %t11, align 4
  %v20 = sext i32 %v19 to i64
  store i64 %v20, ptr %t12, align 8
  %v21 = load i64, ptr %t10, align 8
  %v22 = load i64, ptr %t12, align 8
  %v23 = add i64 %v21, %v22
  store i64 %v23, ptr %t13, align 8
  %v24 = load float, ptr %t3, align 4
  %v25 = call i32 @llvm.fptoui.sat.i32.f32(float %v24)
  store i32 %v25, ptr %t14, align 4
  %v26 = load i32, ptr %t14, align 4
  %v27 = zext i32 %v26 to i64
  store i64 %v27, ptr %t15, align 8
  %v28 = load i64, ptr %t13, align 8
  %v29 = load i64, ptr %t15, align 8
  %v30 = add i64 %v28, %v29
  store i64 %v30, ptr %t16, align 8
  %v31 = load i64, ptr %t16, align 8
  ret i64 %v31
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare i64 @llvm.fptoui.sat.i64.f64(double)
declare i32 @llvm.fptosi.sat.i32.f64(double)
declare i32 @llvm.fptoui.sat.i32.f32(float)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
