source_filename = "structs"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal { i64, double } @structs.call(i64 %p0.0, double %p0.1, ptr byval([24 x i8]) align 8 %p1, i64 %p2.0, <2 x float> %p3.0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca ptr, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  %t11 = alloca i64, align 8
  %t12 = alloca i64, align 8
  %a0 = alloca [16 x i8], align 8
  %a1 = alloca [8 x i8], align 8
  %a2 = alloca [8 x i8], align 8
  %s4 = alloca [24 x i8], align 8
  %s5 = alloca [16 x i8], align 8
  %a3 = alloca [24 x i8], align 8
  %a4 = alloca [16 x i8], align 8
  %a5 = alloca [8 x i8], align 8
  %a6 = alloca [8 x i8], align 8
  %a7 = alloca [16 x i8], align 8
  %a8 = alloca [16 x i8], align 8
  store i64 %p0.0, ptr %a0, align 8
  %v0 = getelementptr i8, ptr %a0, i64 8
  store double %p0.1, ptr %v0, align 8
  store ptr %a0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store i64 %p2.0, ptr %a1, align 8
  store ptr %a1, ptr %t2, align 8
  store <2 x float> %p3.0, ptr %a2, align 8
  store ptr %a2, ptr %t3, align 8
  store ptr %s4, ptr %t4, align 8
  store ptr %s5, ptr %t5, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = load i64, ptr %v1, align 8
  store i64 %v2, ptr %t6, align 8
  %v3 = load i64, ptr %t6, align 8
  call void (ptr, i64) @make(ptr sret([24 x i8]) align 8 %a3, i64 %v3)
  store ptr %a3, ptr %t7, align 8
  %v4 = load ptr, ptr %t4, align 8
  %v5 = load ptr, ptr %t7, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v4, ptr %v5, i64 24, i1 false)
  %v6 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a4, ptr %v6, i64 16, i1 false)
  %v7 = load i64, ptr %a4, align 8
  %v8 = getelementptr i8, ptr %a4, i64 8
  %v9 = load double, ptr %v8, align 8
  %v10 = load ptr, ptr %t4, align 8
  %v11 = load ptr, ptr %t2, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a5, ptr %v11, i64 3, i1 false)
  %v12 = load i64, ptr %a5, align 8
  %v13 = load ptr, ptr %t3, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a6, ptr %v13, i64 8, i1 false)
  %v14 = load <2 x float>, ptr %a6, align 8
  %v15 = call { i64, double } (i64, double, ptr, i64, <2 x float>) @take(i64 %v7, double %v9, ptr byval([24 x i8]) align 8 %v10, i64 %v12, <2 x float> %v14)
  %v16 = extractvalue { i64, double } %v15, 0
  store i64 %v16, ptr %a7, align 8
  %v17 = extractvalue { i64, double } %v15, 1
  %v18 = getelementptr i8, ptr %a7, i64 8
  store double %v17, ptr %v18, align 8
  store ptr %a7, ptr %t8, align 8
  %v19 = load ptr, ptr %t5, align 8
  %v20 = load ptr, ptr %t8, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v19, ptr %v20, i64 16, i1 false)
  %v21 = load ptr, ptr %t5, align 8
  %v22 = load i64, ptr %v21, align 8
  store i64 %v22, ptr %t9, align 8
  %v23 = load ptr, ptr %t1, align 8
  %v24 = getelementptr i8, ptr %v23, i64 16
  store ptr %v24, ptr %t10, align 8
  %v25 = load ptr, ptr %t10, align 8
  %v26 = load i64, ptr %v25, align 8
  store i64 %v26, ptr %t11, align 8
  %v27 = load i64, ptr %t9, align 8
  %v28 = load i64, ptr %t11, align 8
  %v29 = add i64 %v27, %v28
  store i64 %v29, ptr %t12, align 8
  %v30 = load i64, ptr %t12, align 8
  %v31 = load ptr, ptr %t5, align 8
  store i64 %v30, ptr %v31, align 8
  %v32 = load ptr, ptr %t5, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a8, ptr %v32, i64 16, i1 false)
  %v33 = load i64, ptr %a8, align 8
  %v34 = getelementptr i8, ptr %a8, i64 8
  %v35 = load double, ptr %v34, align 8
  %v36 = insertvalue { i64, double } poison, i64 %v33, 0
  %v37 = insertvalue { i64, double } %v36, double %v35, 1
  ret { i64, double } %v37
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare { i64, double } @take(i64, double, ptr byval([24 x i8]) align 8, i64, <2 x float>) #1
declare void @make(ptr sret([24 x i8]) align 8, i64 noundef) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
