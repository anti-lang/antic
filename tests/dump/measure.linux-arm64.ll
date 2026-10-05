source_filename = "measure"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-unknown-linux-gnu"

define internal noundef i64 @measure.length_of([2 x i64] %p0.0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  %a0 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  ret i64 %v4
}

define internal noundef i64 @measure.sum([2 x float] %p0.0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca float, align 4
  %t2 = alloca ptr, align 8
  %t3 = alloca float, align 4
  %t4 = alloca float, align 4
  %t5 = alloca i64, align 8
  %a0 = alloca [8 x i8], align 8
  store [2 x float] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load float, ptr %v0, align 4
  store float %v1, ptr %t1, align 4
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr inbounds i8, ptr %v2, i64 4
  store ptr %v3, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  %v5 = load float, ptr %v4, align 4
  store float %v5, ptr %t3, align 4
  %v6 = load float, ptr %t1, align 4
  %v7 = load float, ptr %t3, align 4
  %v8 = fadd float %v6, %v7
  store float %v8, ptr %t4, align 4
  %v9 = load float, ptr %t4, align 4
  %v10 = call i64 @llvm.fptosi.sat.i64.f32(float %v9)
  store i64 %v10, ptr %t5, align 8
  %v11 = load i64, ptr %t5, align 8
  ret i64 %v11
}

define internal noundef i64 @measure.main() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %s0 = alloca [16 x i8], align 8
  %s1 = alloca [8 x i8], align 4
  %a0 = alloca [16 x i8], align 8
  %a1 = alloca [8 x i8], align 8
  store ptr %s0, ptr %t0, align 8
  store ptr %s1, ptr %t1, align 8
  store ptr @measure.length_of, ptr %t2, align 8
  store ptr @measure.sum, ptr %t3, align 8
  store ptr @measure.6, ptr %t4, align 8
  %v0 = load ptr, ptr %t4, align 8
  %v1 = load ptr, ptr %t0, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t5, align 8
  %v4 = load ptr, ptr %t5, align 8
  store i64 4, ptr %v4, align 8
  %v5 = load ptr, ptr %t2, align 8
  %v6 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a0, ptr %v6, i64 16, i1 false)
  %v7 = load [2 x i64], ptr %a0, align 8
  %v8 = call i64 ([2 x i64]) %v5([2 x i64] %v7)
  store i64 %v8, ptr %t6, align 8
  %v9 = load ptr, ptr %t1, align 8
  store float 0x3FF8000000000000, ptr %v9, align 4
  %v10 = load ptr, ptr %t1, align 8
  %v11 = getelementptr i8, ptr %v10, i64 4
  store ptr %v11, ptr %t7, align 8
  %v12 = load ptr, ptr %t7, align 8
  store float 0x3FF8000000000000, ptr %v12, align 4
  %v13 = load ptr, ptr %t3, align 8
  %v14 = load ptr, ptr %t1, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v14, i64 8, i1 false)
  %v15 = load [2 x float], ptr %a1, align 8
  %v16 = call i64 ([2 x float]) %v13([2 x float] %v15)
  store i64 %v16, ptr %t8, align 8
  %v17 = load i64, ptr %t6, align 8
  %v18 = load i64, ptr %t8, align 8
  %v19 = add i64 %v17, %v18
  store i64 %v19, ptr %t9, align 8
  %v20 = load i64, ptr %t9, align 8
  ret i64 %v20
}

@anti.rt.main = alias i64 (), ptr @measure.main

@measure.6 = internal constant <{ [5 x i8] }> <{ [5 x i8] c"four\00" }>, align 1
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i64 @measure.fn.0([2 x i64]) #1
declare noundef i64 @measure.fn.1([2 x float]) #1
declare i64 @llvm.fptosi.sat.i64.f32(float)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+fp-armv8,+neon,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
