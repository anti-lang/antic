source_filename = "c_callback"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define dso_local noundef float @run(ptr noundef %p0, ptr noundef %p1, float noundef %p2) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca float, align 4
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca float, align 4
  %s3 = alloca [16 x i8], align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store float %p2, ptr %t2, align 4
  store ptr %s3, ptr %t3, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %t3, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t3, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t4, align 8
  %v4 = load ptr, ptr %t1, align 8
  %v5 = load ptr, ptr %t4, align 8
  store ptr %v4, ptr %v5, align 8
  store ptr @c_callback.callback.0, ptr %t5, align 8
  %v6 = load ptr, ptr %t5, align 8
  %v7 = load float, ptr %t2, align 4
  %v8 = load ptr, ptr %t3, align 8
  %v10 = icmp eq ptr %v8, null
  br i1 %v10, label %n9, label %j9

n9:
  %v11 = fcmp uno float %v7, 0.0
  %v12 = select i1 %v11, float 0x7FF8000000000000, float %v7
  br label %j9

j9:
  %v13 = phi float [ %v12, %n9 ], [ %v7, %b0 ]
  %v14 = load ptr, ptr %t3, align 8
  %v15 = call float (float, ptr) %v6(float %v13, ptr %v14)
  store float %v15, ptr %t6, align 4
  %v16 = load float, ptr %t6, align 4
  %v17 = fcmp uno float %v16, 0.0
  %v18 = select i1 %v17, float 0x7FF8000000000000, float %v16
  ret float %v18
}

define internal noundef i64 @c_callback.main() noinline #0 {
b0:
  ret i64 0
}

define internal noundef float @c_callback.callback.0(float noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca float, align 4
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca float, align 4
  store float %p0, ptr %t0, align 4
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %v0, align 8
  store ptr %v1, ptr %t2, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t3, align 8
  %v4 = load ptr, ptr %t3, align 8
  %v5 = load ptr, ptr %v4, align 8
  store ptr %v5, ptr %t4, align 8
  %v6 = load ptr, ptr %t2, align 8
  %v7 = load float, ptr %t0, align 4
  %v8 = fcmp uno float %v7, 0.0
  %v9 = select i1 %v8, float 0x7FF8000000000000, float %v7
  %v10 = load ptr, ptr %t4, align 8
  %v11 = call float (float, ptr) %v6(float %v9, ptr %v10)
  store float %v11, ptr %t5, align 4
  %v12 = load float, ptr %t5, align 4
  ret float %v12
}

@anti.rt.main = alias i64 (), ptr @c_callback.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8
@llvm.compiler.used = appending global [1 x ptr] [ptr @c_callback.main], section "llvm.metadata"

declare noundef float @c_callback.fn.0(float noundef, ptr noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
