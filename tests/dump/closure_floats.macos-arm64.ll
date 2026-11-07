source_filename = "closure_floats"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef float @closure_floats.apply(ptr noundef %p0, ptr noundef %p1, float noundef %p2) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca float, align 4
  %t3 = alloca float, align 4
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store float %p2, ptr %t2, align 4
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load float, ptr %t2, align 4
  %v2 = load ptr, ptr %t1, align 8
  %v4 = icmp eq ptr %v2, null
  br i1 %v4, label %n3, label %j3

n3:
  %v5 = fcmp uno float %v1, 0.0
  %v6 = select i1 %v5, float 0x7FF8000000000000, float %v1
  br label %j3

j3:
  %v7 = phi float [ %v6, %n3 ], [ %v1, %b0 ]
  %v8 = load ptr, ptr %t1, align 8
  %v9 = call float (float, ptr) %v0(float %v7, ptr %v8)
  store float %v9, ptr %t3, align 4
  %v10 = load float, ptr %t3, align 4
  ret float %v10
}

define internal noundef i64 @closure_floats.count(ptr noundef %p0, ptr noundef %p1, i64 noundef %p2) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store i64 %p2, ptr %t2, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i64, ptr %t2, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = call i64 (i64, ptr) %v0(i64 %v1, ptr %v2)
  store i64 %v3, ptr %t3, align 8
  %v4 = load i64, ptr %t3, align 8
  ret i64 %v4
}

define internal noundef float @closure_floats.scaled(float noundef %p0, float noundef %p1) #0 {
b0:
  %t0 = alloca float, align 4
  %t1 = alloca float, align 4
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca float, align 4
  %s2 = alloca [4 x i8], align 4
  %s3 = alloca [8 x i8], align 8
  store float %p0, ptr %t0, align 4
  store float %p1, ptr %t1, align 4
  store ptr %s2, ptr %t2, align 8
  store ptr %s3, ptr %t3, align 8
  %v0 = load float, ptr %t1, align 4
  %v1 = load ptr, ptr %t2, align 8
  %v2 = fcmp uno float %v0, 0.0
  %v3 = select i1 %v2, float 0x7FF8000000000000, float %v0
  store float %v3, ptr %v1, align 4
  store ptr @closure_floats.scaled.0, ptr %t4, align 8
  %v4 = load ptr, ptr %t2, align 8
  %v5 = load ptr, ptr %t3, align 8
  store ptr %v4, ptr %v5, align 8
  %v6 = load ptr, ptr %t4, align 8
  %v7 = load ptr, ptr %t3, align 8
  %v8 = load float, ptr %t0, align 4
  %v9 = call float (ptr, ptr, float) @closure_floats.apply(ptr %v6, ptr %v7, float %v8)
  store float %v9, ptr %t5, align 4
  %v10 = load float, ptr %t5, align 4
  ret float %v10
}

define internal noundef i64 @closure_floats.main() noinline #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca float, align 4
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  store ptr @closure_floats.main.1, ptr %t0, align 8
  %v0 = call float (float, float) @closure_floats.scaled(float 0x4000000000000000, float 0x4008000000000000)
  store float %v0, ptr %t1, align 4
  %v1 = load float, ptr %t1, align 4
  %v2 = call i64 @llvm.fptosi.sat.i64.f32(float %v1)
  store i64 %v2, ptr %t2, align 8
  %v3 = load ptr, ptr %t0, align 8
  %v4 = load i64, ptr %t2, align 8
  %v5 = call i64 (ptr, ptr, i64) @closure_floats.count(ptr %v3, ptr null, i64 %v4)
  store i64 %v5, ptr %t3, align 8
  %v6 = load i64, ptr %t3, align 8
  %v7 = sub i64 %v6, 7
  store i64 %v7, ptr %t4, align 8
  %v8 = load i64, ptr %t4, align 8
  ret i64 %v8
}

define internal noundef float @closure_floats.scaled.0(float noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca float, align 4
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca float, align 4
  %t4 = alloca float, align 4
  store float %p0, ptr %t0, align 4
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %v0, align 8
  store ptr %v1, ptr %t2, align 8
  %v2 = load ptr, ptr %t2, align 8
  %v3 = load float, ptr %v2, align 4, !tbaa !16
  store float %v3, ptr %t3, align 4
  %v4 = load float, ptr %t0, align 4
  %v5 = load float, ptr %t3, align 4
  %v6 = fmul float %v4, %v5
  store float %v6, ptr %t4, align 4
  %v7 = load float, ptr %t4, align 4
  %v8 = fcmp uno float %v7, 0.0
  %v9 = select i1 %v8, float 0x7FF8000000000000, float %v7
  ret float %v9
}

define internal noundef i64 @closure_floats.main.1(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = add i64 %v0, 1
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t1, align 8
  ret i64 %v2
}

@anti.rt.main = alias i64 (), ptr @closure_floats.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8
@llvm.compiler.used = appending global [1 x ptr] [ptr @closure_floats.main], section "llvm.metadata"

declare noundef float @closure_floats.fn.0(float noundef, ptr noundef) #1
declare noundef i64 @closure_floats.fn.1(i64 noundef, ptr noundef) #1
declare i64 @llvm.fptosi.sat.i64.f32(float)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!5 = !{!"anti"}
!6 = !{!"byte", !5, i64 0}
!7 = !{!"i16", !6, i64 0}
!8 = !{!"i32", !6, i64 0}
!9 = !{!"i64", !6, i64 0}
!10 = !{!"f32", !6, i64 0}
!11 = !{!"f64", !6, i64 0}
!12 = !{!"ptr", !6, i64 0}
!13 = !{!7, !7, i64 0}
!14 = !{!8, !8, i64 0}
!15 = !{!9, !9, i64 0}
!16 = !{!10, !10, i64 0}
!17 = !{!11, !11, i64 0}
!18 = !{!12, !12, i64 0}
