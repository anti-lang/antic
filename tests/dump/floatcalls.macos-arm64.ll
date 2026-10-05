source_filename = "floatcalls"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef double @floatcalls.calls(ptr noundef nonnull dereferenceable(1) %p0, double noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca double, align 8
  %t2 = alloca i32, align 4
  %t3 = alloca double, align 8
  %t4 = alloca float, align 4
  %t5 = alloca double, align 8
  %t6 = alloca double, align 8
  store ptr %p0, ptr %t0, align 8
  store double %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load double, ptr %t1, align 8
  %v2 = call i32 (ptr, ...) @printf(ptr %v0, double %v1, i64 3)
  store i32 %v2, ptr %t2, align 4
  %v3 = load double, ptr %t1, align 8
  %v4 = call double (i64, double, i64, float) @mix(i64 1, double %v3, i64 2, float 0x3FE0000000000000)
  store double %v4, ptr %t3, align 8
  %v5 = load double, ptr %t1, align 8
  %v6 = load double, ptr %t1, align 8
  %v7 = load double, ptr %t1, align 8
  %v8 = load double, ptr %t1, align 8
  %v9 = load double, ptr %t1, align 8
  %v10 = load double, ptr %t1, align 8
  %v11 = load double, ptr %t1, align 8
  %v12 = load double, ptr %t1, align 8
  %v13 = call float (double, double, double, double, double, double, double, double, float, float) @nine(double %v5, double %v6, double %v7, double %v8, double %v9, double %v10, double %v11, double %v12, float 0x3FF0000000000000, float 0x4000000000000000)
  store float %v13, ptr %t4, align 4
  %v14 = load float, ptr %t4, align 4
  %v15 = fpext float %v14 to double
  store double %v15, ptr %t5, align 8
  %v16 = load double, ptr %t3, align 8
  %v17 = load double, ptr %t5, align 8
  %v18 = fadd double %v16, %v17
  store double %v18, ptr %t6, align 8
  %v19 = load double, ptr %t6, align 8
  ret double %v19
}

define internal noundef float @floatcalls.last(double noundef %p0, double noundef %p1, double noundef %p2, double noundef %p3, double noundef %p4, double noundef %p5, double noundef %p6, double noundef %p7, float noundef %p8, float noundef %p9) #0 {
b0:
  %t0 = alloca double, align 8
  %t1 = alloca double, align 8
  %t2 = alloca double, align 8
  %t3 = alloca double, align 8
  %t4 = alloca double, align 8
  %t5 = alloca double, align 8
  %t6 = alloca double, align 8
  %t7 = alloca double, align 8
  %t8 = alloca float, align 4
  %t9 = alloca float, align 4
  store double %p0, ptr %t0, align 8
  store double %p1, ptr %t1, align 8
  store double %p2, ptr %t2, align 8
  store double %p3, ptr %t3, align 8
  store double %p4, ptr %t4, align 8
  store double %p5, ptr %t5, align 8
  store double %p6, ptr %t6, align 8
  store double %p7, ptr %t7, align 8
  store float %p8, ptr %t8, align 4
  store float %p9, ptr %t9, align 4
  %v0 = load float, ptr %t9, align 4
  ret float %v0
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @printf(ptr noundef, ...) #1
declare noundef double @mix(i64 noundef, double noundef, i64 noundef, float noundef) #1
declare noundef float @nine(double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, float noundef, float noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
