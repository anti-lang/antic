source_filename = "effects"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define hidden noundef zeroext range(i8 0, 2) i8 @effects.next(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i64, ptr %v0, align 8
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t1, align 8
  %v3 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %v2, i64 1)
  %v4 = add nsw i64 %v2, 1
  %v5 = extractvalue { i64, i1 } %v3, 1
  store i64 %v4, ptr %t2, align 8
  br i1 %v5, label %b1, label %b2, !prof !3

b1:
  store ptr @effects.5, ptr %t3, align 8
  %v6 = load ptr, ptr %t3, align 8
  %v7 = load i64, ptr %t1, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v6, i64 30, i32 1, i64 %v7, i64 1)
  unreachable

b2:
  %v8 = load i64, ptr %t2, align 8
  %v9 = load ptr, ptr %t0, align 8
  store i64 %v8, ptr %v9, align 8
  %v10 = load i64, ptr %t2, align 8
  %v11 = icmp sle i64 %v10, 3
  %v12 = zext i1 %v11 to i8
  store i8 %v12, ptr %t4, align 1
  %v13 = load i8, ptr %t4, align 1
  ret i8 %v13
}

define hidden noundef i64 @effects.value(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i64, ptr %v0, align 8
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t1, align 8
  ret i64 %v2
}

define hidden noundef i64 @effects.texts(i64 %p0.0, i64 %p0.1, i64 %p1.0, i64 %p1.1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i32, align 4
  %t9 = alloca i8, align 1
  %t10 = alloca ptr, align 8
  %t11 = alloca ptr, align 8
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca ptr, align 8
  %t15 = alloca ptr, align 8
  %t16 = alloca i64, align 8
  %t17 = alloca ptr, align 8
  %t18 = alloca ptr, align 8
  %t19 = alloca i64, align 8
  %t20 = alloca i64, align 8
  %t21 = alloca i8, align 1
  %a0 = alloca [16 x i8], align 8
  %a1 = alloca [16 x i8], align 8
  store i64 %p0.0, ptr %a0, align 8
  %v0 = getelementptr i8, ptr %a0, i64 8
  store i64 %p0.1, ptr %v0, align 8
  store ptr %a0, ptr %t0, align 8
  store i64 %p1.0, ptr %a1, align 8
  %v1 = getelementptr i8, ptr %a1, i64 8
  store i64 %p1.1, ptr %v1, align 8
  store ptr %a1, ptr %t1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = load ptr, ptr %v2, align 8
  store ptr %v3, ptr %t2, align 8
  %v4 = load ptr, ptr %t0, align 8
  %v5 = getelementptr i8, ptr %v4, i64 8
  store ptr %v5, ptr %t3, align 8
  %v6 = load ptr, ptr %t3, align 8
  %v7 = load i64, ptr %v6, align 8
  store i64 %v7, ptr %t4, align 8
  %v8 = load ptr, ptr %t1, align 8
  %v9 = load ptr, ptr %v8, align 8
  store ptr %v9, ptr %t5, align 8
  %v10 = load ptr, ptr %t1, align 8
  %v11 = getelementptr i8, ptr %v10, i64 8
  store ptr %v11, ptr %t6, align 8
  %v12 = load ptr, ptr %t6, align 8
  %v13 = load i64, ptr %v12, align 8
  store i64 %v13, ptr %t7, align 8
  %v14 = load ptr, ptr %t2, align 8
  %v15 = load i64, ptr %t4, align 8
  %v16 = load ptr, ptr %t5, align 8
  %v17 = load i64, ptr %t7, align 8
  %v18 = call i32 (ptr, i64, ptr, i64) @anti_rt_same_bytes(ptr %v14, i64 %v15, ptr %v16, i64 %v17)
  store i32 %v18, ptr %t8, align 4
  %v19 = load i32, ptr %t8, align 4
  %v20 = icmp ne i32 %v19, 0
  %v21 = zext i1 %v20 to i8
  store i8 %v21, ptr %t9, align 1
  %v22 = load i8, ptr %t9, align 1
  %v23 = trunc i8 %v22 to i1
  br i1 %v23, label %b1, label %b3

b1:
  ret i64 1

b2:
  %v24 = load ptr, ptr %t0, align 8
  %v25 = load ptr, ptr %v24, align 8
  store ptr %v25, ptr %t10, align 8
  %v26 = load ptr, ptr %t0, align 8
  %v27 = getelementptr i8, ptr %v26, i64 8
  store ptr %v27, ptr %t11, align 8
  %v28 = load ptr, ptr %t11, align 8
  %v29 = load i64, ptr %v28, align 8
  store i64 %v29, ptr %t12, align 8
  %v30 = load ptr, ptr %t10, align 8
  %v31 = load i64, ptr %t12, align 8
  %v32 = call i64 (ptr, i64) @anti_rt_hash_bytes(ptr %v30, i64 %v31)
  store i64 %v32, ptr %t13, align 8
  %v33 = load i64, ptr %t13, align 8
  ret i64 %v33

b3:
  %v34 = load ptr, ptr %t0, align 8
  %v35 = load ptr, ptr %v34, align 8
  store ptr %v35, ptr %t14, align 8
  %v36 = load ptr, ptr %t0, align 8
  %v37 = getelementptr i8, ptr %v36, i64 8
  store ptr %v37, ptr %t15, align 8
  %v38 = load ptr, ptr %t15, align 8
  %v39 = load i64, ptr %v38, align 8
  store i64 %v39, ptr %t16, align 8
  %v40 = load ptr, ptr %t1, align 8
  %v41 = load ptr, ptr %v40, align 8
  store ptr %v41, ptr %t17, align 8
  %v42 = load ptr, ptr %t1, align 8
  %v43 = getelementptr i8, ptr %v42, i64 8
  store ptr %v43, ptr %t18, align 8
  %v44 = load ptr, ptr %t18, align 8
  %v45 = load i64, ptr %v44, align 8
  store i64 %v45, ptr %t19, align 8
  %v46 = load ptr, ptr %t14, align 8
  %v47 = load i64, ptr %t16, align 8
  %v48 = load ptr, ptr %t17, align 8
  %v49 = load i64, ptr %t19, align 8
  %v50 = call i64 (ptr, i64, ptr, i64) @anti_rt_compare_bytes(ptr %v46, i64 %v47, ptr %v48, i64 %v49)
  store i64 %v50, ptr %t20, align 8
  %v51 = load i64, ptr %t20, align 8
  %v52 = icmp slt i64 %v51, 0
  %v53 = zext i1 %v52 to i8
  store i8 %v53, ptr %t21, align 1
  %v54 = load i8, ptr %t21, align 1
  %v55 = trunc i8 %v54 to i1
  br i1 %v55, label %b1, label %b2
}

define hidden noundef float @effects.halves(float noundef %p0) #0 {
b0:
  %t0 = alloca float, align 4
  %t1 = alloca i16, align 2
  %t2 = alloca float, align 4
  store float %p0, ptr %t0, align 4
  %v0 = load float, ptr %t0, align 4
  %v1 = call i32 @anti_rt_f32_to_f16(float %v0)
  %v2 = trunc i32 %v1 to i16
  store i16 %v2, ptr %t1, align 2
  %v3 = load i16, ptr %t1, align 2
  %v4 = zext i16 %v3 to i32
  %v5 = call float @anti_rt_f16_to_f32(i32 %v4)
  store float %v5, ptr %t2, align 4
  %v6 = load float, ptr %t2, align 4
  ret float %v6
}

define hidden { i64, i64 } @effects.collect() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i8, align 1
  %t6 = alloca i8, align 1
  %t7 = alloca i64, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  %t11 = alloca i64, align 8
  %t12 = alloca ptr, align 8
  %s0 = alloca [16 x i8], align 8
  %s1 = alloca [8 x i8], align 8
  %a0 = alloca [16 x i8], align 8
  store ptr %s0, ptr %t0, align 8
  store ptr %s1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  store i64 0, ptr %v0, align 8
  store ptr null, ptr %t2, align 8
  store i64 0, ptr %t3, align 8
  store i64 0, ptr %t4, align 8
  br label %b1

b1:
  %v1 = load ptr, ptr %t1, align 8
  %v2 = call range(i8 0, 2) i8 (ptr) @effects.next(ptr %v1)
  store i8 %v2, ptr %t5, align 1
  %v3 = load i8, ptr %t5, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b2, label %b5

b2:
  %v5 = load i64, ptr %t3, align 8
  %v6 = load i64, ptr %t4, align 8
  %v7 = icmp eq i64 %v5, %v6
  %v8 = zext i1 %v7 to i8
  store i8 %v8, ptr %t6, align 1
  %v9 = load i8, ptr %t6, align 1
  %v10 = trunc i8 %v9 to i1
  br i1 %v10, label %b3, label %b4

b3:
  %v11 = load i64, ptr %t4, align 8
  %v12 = and i64 1, 63
  %v13 = shl i64 %v11, %v12
  store i64 %v13, ptr %t7, align 8
  %v14 = load i64, ptr %t7, align 8
  %v15 = add i64 %v14, 4
  store i64 %v15, ptr %t4, align 8
  %v16 = load i64, ptr %t4, align 8
  %v17 = and i64 3, 63
  %v18 = shl i64 %v16, %v17
  store i64 %v18, ptr %t8, align 8
  %v19 = load ptr, ptr %t2, align 8
  %v20 = load i64, ptr %t8, align 8
  %v21 = call ptr (ptr, i64) @anti_rt_grow(ptr %v19, i64 %v20)
  store ptr %v21, ptr %t2, align 8
  br label %b4

b4:
  %v22 = load i64, ptr %t3, align 8
  %v23 = and i64 3, 63
  %v24 = shl i64 %v22, %v23
  store i64 %v24, ptr %t9, align 8
  %v25 = load ptr, ptr %t2, align 8
  %v26 = load i64, ptr %t9, align 8
  %v27 = getelementptr i8, ptr %v25, i64 %v26
  store ptr %v27, ptr %t10, align 8
  %v28 = load ptr, ptr %t1, align 8
  %v29 = call i64 (ptr) @effects.value(ptr %v28)
  store i64 %v29, ptr %t11, align 8
  %v30 = load i64, ptr %t11, align 8
  %v31 = load ptr, ptr %t10, align 8
  store i64 %v30, ptr %v31, align 8
  %v32 = load i64, ptr %t3, align 8
  %v33 = add i64 %v32, 1
  store i64 %v33, ptr %t3, align 8
  br label %b1

b5:
  %v34 = load ptr, ptr %t2, align 8
  %v35 = load ptr, ptr %t0, align 8
  store ptr %v34, ptr %v35, align 8
  %v36 = load ptr, ptr %t0, align 8
  %v37 = getelementptr i8, ptr %v36, i64 8
  store ptr %v37, ptr %t12, align 8
  %v38 = load i64, ptr %t3, align 8
  %v39 = load ptr, ptr %t12, align 8
  store i64 %v38, ptr %v39, align 8
  %v40 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a0, ptr %v40, i64 16, i1 false)
  %v41 = load i64, ptr %a0, align 8
  %v42 = getelementptr i8, ptr %a0, i64 8
  %v43 = load i64, ptr %v42, align 8
  %v44 = insertvalue { i64, i64 } poison, i64 %v41, 0
  %v45 = insertvalue { i64, i64 } %v44, i64 %v43, 1
  ret { i64, i64 } %v45
}

define hidden void @effects.hold(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  call void (ptr) @anti_rt_snapshot_free(ptr %v0)
  ret void
}

define hidden void @effects.snapshot(i64 %p0.0, i64 %p0.1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca ptr, align 8
  %t10 = alloca ptr, align 8
  %a0 = alloca [16 x i8], align 8
  store i64 %p0.0, ptr %a0, align 8
  %v0 = getelementptr i8, ptr %a0, i64 8
  store i64 %p0.1, ptr %v0, align 8
  store ptr %a0, ptr %t0, align 8
  store ptr @effects.snapshot.0, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = getelementptr i8, ptr %v1, i64 8
  store ptr %v2, ptr %t2, align 8
  %v3 = load ptr, ptr %t2, align 8
  %v4 = load i64, ptr %v3, align 8
  store i64 %v4, ptr %t3, align 8
  %v5 = load i64, ptr %t3, align 8
  %v6 = add i64 %v5, 24
  store i64 %v6, ptr %t4, align 8
  %v7 = load i64, ptr %t4, align 8
  %v8 = call ptr (i64) @anti_rt_snapshot_new(i64 %v7)
  store ptr %v8, ptr %t5, align 8
  %v9 = load ptr, ptr %t5, align 8
  %v10 = getelementptr i8, ptr %v9, i64 8
  store ptr %v10, ptr %t6, align 8
  %v11 = load ptr, ptr %t0, align 8
  %v12 = getelementptr i8, ptr %v11, i64 8
  store ptr %v12, ptr %t7, align 8
  %v13 = load ptr, ptr %t7, align 8
  %v14 = load i64, ptr %v13, align 8
  store i64 %v14, ptr %t8, align 8
  %v15 = load ptr, ptr %t0, align 8
  %v16 = load ptr, ptr %v15, align 8
  store ptr %v16, ptr %t9, align 8
  %v17 = load ptr, ptr %t5, align 8
  %v18 = load ptr, ptr %t9, align 8
  %v19 = load i64, ptr %t8, align 8
  call void (ptr, i64, ptr, i64) @anti_rt_snapshot_text(ptr %v17, i64 24, ptr %v18, i64 %v19)
  %v20 = load ptr, ptr %t6, align 8
  store i64 24, ptr %v20, align 8
  %v21 = load ptr, ptr %t6, align 8
  %v22 = getelementptr i8, ptr %v21, i64 8
  store ptr %v22, ptr %t10, align 8
  %v23 = load i64, ptr %t8, align 8
  %v24 = load ptr, ptr %t10, align 8
  store i64 %v23, ptr %v24, align 8
  %v25 = load ptr, ptr %t1, align 8
  %v26 = load ptr, ptr %t5, align 8
  call void (ptr, ptr) @effects.hold(ptr %v25, ptr %v26)
  ret void
}

define hidden void @effects.snapshot.0(i64 noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca ptr, align 8
  %s2 = alloca [16 x i8], align 8
  %s4 = alloca [16 x i8], align 8
  store i64 %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store ptr %s2, ptr %t2, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = getelementptr i8, ptr %v0, i64 8
  store ptr %v1, ptr %t3, align 8
  store ptr %s4, ptr %t4, align 8
  %v2 = load ptr, ptr %t3, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t5, align 8
  %v4 = load ptr, ptr %t1, align 8
  %v5 = load i64, ptr %t5, align 8
  %v6 = getelementptr i8, ptr %v4, i64 %v5
  store ptr %v6, ptr %t6, align 8
  %v7 = load ptr, ptr %t6, align 8
  %v8 = load ptr, ptr %t4, align 8
  store ptr %v7, ptr %v8, align 8
  %v9 = load ptr, ptr %t3, align 8
  %v10 = getelementptr i8, ptr %v9, i64 8
  store ptr %v10, ptr %t7, align 8
  %v11 = load ptr, ptr %t7, align 8
  %v12 = load i64, ptr %v11, align 8
  store i64 %v12, ptr %t8, align 8
  %v13 = load ptr, ptr %t4, align 8
  %v14 = getelementptr i8, ptr %v13, i64 8
  store ptr %v14, ptr %t9, align 8
  %v15 = load i64, ptr %t8, align 8
  %v16 = load ptr, ptr %t9, align 8
  store i64 %v15, ptr %v16, align 8
  call void @llvm.lifetime.start.p0(ptr %s2)
  %v17 = load ptr, ptr %t2, align 8
  %v18 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v17, ptr %v18, i64 16, i1 false)
  call void @llvm.lifetime.end.p0(ptr %s2)
  ret void
}

@effects.Count.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @effects.1, [48 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @effects.Count.fields, [32 x i8] zeroinitializer, ptr @effects.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@effects.1 = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"Count\00" }>, align 1
@effects.2 = hidden constant <{ [3 x i8] }> <{ [3 x i8] c"at\00" }>, align 1
@effects.Count.fields = hidden constant <{ ptr, [40 x i8] }> <{ ptr @effects.2, [40 x i8] c"\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@effects.package.version = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@effects.5 = hidden constant <{ [31 x i8] }> <{ [31 x i8] c"effects.anti:14: overflow in +\00" }>, align 1

declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare noundef i32 @anti_rt_same_bytes(ptr noundef, i64 noundef, ptr noundef, i64 noundef) memory(argmem: read) willreturn nosync nofree #1
declare noundef i64 @anti_rt_compare_bytes(ptr noundef, i64 noundef, ptr noundef, i64 noundef) memory(argmem: read) willreturn nosync nofree #1
declare noundef i64 @anti_rt_hash_bytes(ptr noundef, i64 noundef) memory(argmem: read) willreturn nosync nofree #1
declare noundef ptr @anti_rt_grow(ptr noundef, i64 noundef) memory(argmem: readwrite, inaccessiblemem: readwrite, errnomem: write) #1
declare void @anti_rt_snapshot_free(ptr noundef) #1
declare noundef ptr @anti_rt_snapshot_new(i64 noundef) #1
declare void @anti_rt_snapshot_text(ptr noundef, i64 noundef, ptr noundef, i64 noundef) memory(argmem: readwrite, inaccessiblemem: readwrite, errnomem: write) willreturn nosync nofree #1
declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, i64)
declare i32 @anti_rt_f32_to_f16(float) memory(none) willreturn nosync nofree
declare float @anti_rt_f16_to_f32(i32) memory(none) willreturn nosync nofree
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.lifetime.start.p0(ptr)
declare void @llvm.lifetime.end.p0(ptr)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v2" "target-features"="+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
