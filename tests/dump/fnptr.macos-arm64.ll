source_filename = "fnptr"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i32 @fnptr.twice(i32 noundef %p0) #0 {
b0:
  %t0 = alloca i32, align 4
  %t1 = alloca i32, align 4
  store i32 %p0, ptr %t0, align 4
  %v0 = load i32, ptr %t0, align 4
  %v1 = and i32 1, 31
  %v2 = shl i32 %v0, %v1
  store i32 %v2, ptr %t1, align 4
  %v3 = load i32, ptr %t1, align 4
  ret i32 %v3
}

define internal noundef i32 @fnptr.run(ptr noundef nonnull dereferenceable(8) %p0, i32 noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i32, align 4
  %t2 = alloca ptr, align 8
  %t3 = alloca i32, align 4
  store ptr %p0, ptr %t0, align 8
  store i32 %p1, ptr %t1, align 4
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %v0, align 8
  store ptr %v1, ptr %t2, align 8
  %v2 = load ptr, ptr %t2, align 8
  %v3 = load i32, ptr %t1, align 4
  %v4 = call i32 (i32) %v2(i32 %v3)
  store i32 %v4, ptr %t3, align 4
  %v5 = load i32, ptr %t3, align 4
  ret i32 %v5
}

define internal noundef i64 @fnptr.main() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca i32, align 4
  %t6 = alloca i64, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca i32, align 4
  %t9 = alloca i64, align 8
  %t10 = alloca i64, align 8
  %s0 = alloca [16 x i8], align 8
  store ptr %s0, ptr %t0, align 8
  call void @llvm.lifetime.start.p0(ptr %s0)
  store ptr @fnptr.twice, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t2, align 8
  store ptr @abs, ptr %t3, align 8
  %v4 = load ptr, ptr %t3, align 8
  %v5 = load ptr, ptr %t2, align 8
  store ptr %v4, ptr %v5, align 8
  %v6 = load ptr, ptr %t0, align 8
  %v7 = getelementptr i8, ptr %v6, i64 0
  store ptr %v7, ptr %t4, align 8
  %v8 = load ptr, ptr %t4, align 8
  %v9 = call i32 (ptr, i32) @fnptr.run(ptr %v8, i32 5)
  store i32 %v9, ptr %t5, align 4
  %v10 = load i32, ptr %t5, align 4
  %v11 = sext i32 %v10 to i64
  store i64 %v11, ptr %t6, align 8
  %v12 = load ptr, ptr %t0, align 8
  %v13 = getelementptr i8, ptr %v12, i64 8
  store ptr %v13, ptr %t7, align 8
  %v14 = load ptr, ptr %t7, align 8
  %v15 = call i32 (ptr, i32) @fnptr.run(ptr %v14, i32 -3)
  store i32 %v15, ptr %t8, align 4
  %v16 = load i32, ptr %t8, align 4
  %v17 = sext i32 %v16 to i64
  store i64 %v17, ptr %t9, align 8
  %v18 = load i64, ptr %t6, align 8
  %v19 = load i64, ptr %t9, align 8
  %v20 = add i64 %v18, %v19
  store i64 %v20, ptr %t10, align 8
  %v21 = load i64, ptr %t10, align 8
  ret i64 %v21
}

@anti.rt.main = alias i64 (), ptr @fnptr.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @abs(i32 noundef) #1
declare noundef i32 @fnptr.fn.0(i32 noundef) #1
declare void @llvm.lifetime.start.p0(ptr)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
