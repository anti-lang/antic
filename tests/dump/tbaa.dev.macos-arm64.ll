source_filename = "tbaa"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define hidden noundef double @tbaa.field(ptr noundef nonnull dereferenceable(24) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca double, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  store i64 1, ptr %v0, align 8, !tbaa !23
  %v1 = load ptr, ptr %t0, align 8
  %v2 = getelementptr inbounds i8, ptr %v1, i64 8
  store ptr %v2, ptr %t1, align 8
  %v3 = load ptr, ptr %t1, align 8
  %v4 = load double, ptr %v3, align 8, !tbaa !24
  store double %v4, ptr %t2, align 8
  %v5 = load double, ptr %t2, align 8
  ret double %v5
}

define hidden noundef zeroext i16 @tbaa.element([2 x i64] %p0.0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca i8, align 1
  %t6 = alloca ptr, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca ptr, align 8
  %t9 = alloca i16, align 2
  %a0 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr i8, ptr %v0, i64 8
  store ptr %v1, ptr %t2, align 8
  %v2 = load ptr, ptr %t2, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t3, align 8
  %v4 = load ptr, ptr %t0, align 8
  %v5 = load ptr, ptr %v4, align 8
  store ptr %v5, ptr %t4, align 8
  %v6 = load i64, ptr %t1, align 8
  %v7 = load i64, ptr %t3, align 8
  %v8 = icmp ult i64 %v6, %v7
  %v9 = zext i1 %v8 to i8
  store i8 %v9, ptr %t5, align 1
  %v10 = load i8, ptr %t5, align 1
  %v11 = trunc i8 %v10 to i1
  br i1 %v11, label %b2, label %b1, !prof !4

b1:
  store ptr @tbaa.49, ptr %t6, align 8
  %v12 = load ptr, ptr %t6, align 8
  %v13 = load i64, ptr %t1, align 8
  %v14 = load i64, ptr %t3, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v12, i64 33, i32 0, i64 %v13, i64 %v14)
  unreachable

b2:
  %v15 = load i64, ptr %t1, align 8
  %v16 = and i64 1, 63
  %v17 = shl i64 %v15, %v16
  store i64 %v17, ptr %t7, align 8
  %v18 = load ptr, ptr %t4, align 8
  %v19 = load i64, ptr %t7, align 8
  %v20 = getelementptr i8, ptr %v18, i64 %v19
  store ptr %v20, ptr %t8, align 8
  %v21 = load ptr, ptr %t8, align 8
  %v22 = load i16, ptr %v21, align 2, !tbaa !14
  store i16 %v22, ptr %t9, align 2
  %v23 = load i16, ptr %t9, align 2
  ret i16 %v23
}

define hidden noundef ptr @tbaa.pointer(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %v0, align 8, !tbaa !19
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  ret ptr %v2
}

define hidden noundef float @tbaa.lane(ptr noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca float, align 4
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load float, ptr %v2, align 4, !tbaa !17
  store float %v3, ptr %t2, align 4
  %v4 = load float, ptr %t2, align 4
  ret float %v4
}

define hidden noundef i64 @tbaa.base(ptr noundef nonnull dereferenceable(24) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 16
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  store i32 2, ptr %v2, align 4, !tbaa !25
  %v3 = load ptr, ptr %t0, align 8
  %v4 = getelementptr inbounds i8, ptr %v3, i64 8
  store ptr %v4, ptr %t2, align 8
  %v5 = load ptr, ptr %t2, align 8
  %v6 = load i64, ptr %v5, align 8, !tbaa !26
  store i64 %v6, ptr %t3, align 8
  %v7 = load i64, ptr %t3, align 8
  ret i64 %v7
}

define hidden noundef i64 @tbaa.view(ptr noundef nonnull dereferenceable(24) %p0) #0 {
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

define hidden noundef zeroext range(i8 0, 2) i8 @tbaa.bytes(ptr noundef nonnull dereferenceable(24) %p0, ptr noundef nonnull dereferenceable(1) %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  store i8 0, ptr %v0, align 1
  %v1 = load ptr, ptr %t0, align 8
  %v2 = getelementptr inbounds i8, ptr %v1, i64 16
  store ptr %v2, ptr %t2, align 8
  %v3 = load ptr, ptr %t2, align 8
  %v4 = load i8, ptr %v3, align 1, !range !5
  store i8 %v4, ptr %t3, align 1
  %v5 = load i8, ptr %t3, align 1
  ret i8 %v5
}

define hidden noundef double @tbaa.word(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca double, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  store i64 3, ptr %v0, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = load double, ptr %v1, align 8
  store double %v2, ptr %t1, align 8
  %v3 = load double, ptr %t1, align 8
  ret double %v3
}

define hidden noundef i8 @tbaa.Box.equals(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t5 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i8, align 1
  %t8 = alloca ptr, align 8
  %t9 = alloca ptr, align 8
  %t10 = alloca i64, align 8
  %t11 = alloca i64, align 8
  %t12 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store i8 1, ptr %t2, align 1
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %t1, align 8
  %v2 = icmp eq ptr %v0, %v1
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t3, align 1
  %v4 = load i8, ptr %t3, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b2, label %b3

b1:
  store i8 0, ptr %t2, align 1
  br label %b2

b2:
  %v6 = load i8, ptr %t2, align 1
  ret i8 %v6

b3:
  %v7 = load ptr, ptr %t1, align 8
  %v8 = icmp ne ptr %v7, null
  %v9 = zext i1 %v8 to i8
  store i8 %v9, ptr %t4, align 1
  %v10 = load i8, ptr %t4, align 1
  %v11 = trunc i8 %v10 to i1
  br i1 %v11, label %b4, label %b1

b4:
  %v12 = load ptr, ptr %t0, align 8
  %v13 = call ptr (ptr) @anti_rt_descriptor(ptr %v12)
  store ptr %v13, ptr %t5, align 8
  %v14 = load ptr, ptr %t1, align 8
  %v15 = call ptr (ptr) @anti_rt_descriptor(ptr %v14)
  store ptr %v15, ptr %t6, align 8
  %v16 = load ptr, ptr %t5, align 8
  %v17 = load ptr, ptr %t6, align 8
  %v18 = icmp eq ptr %v16, %v17
  %v19 = zext i1 %v18 to i8
  store i8 %v19, ptr %t7, align 1
  %v20 = load i8, ptr %t7, align 1
  %v21 = trunc i8 %v20 to i1
  br i1 %v21, label %b5, label %b1

b5:
  %v22 = load ptr, ptr %t0, align 8
  %v23 = getelementptr i8, ptr %v22, i64 8
  store ptr %v23, ptr %t8, align 8
  %v24 = load ptr, ptr %t1, align 8
  %v25 = getelementptr i8, ptr %v24, i64 8
  store ptr %v25, ptr %t9, align 8
  %v26 = load ptr, ptr %t8, align 8
  %v27 = load i64, ptr %v26, align 8
  store i64 %v27, ptr %t10, align 8
  %v28 = load ptr, ptr %t9, align 8
  %v29 = load i64, ptr %v28, align 8
  store i64 %v29, ptr %t11, align 8
  %v30 = load i64, ptr %t10, align 8
  %v31 = load i64, ptr %t11, align 8
  %v32 = icmp eq i64 %v30, %v31
  %v33 = zext i1 %v32 to i8
  store i8 %v33, ptr %t12, align 1
  %v34 = load i8, ptr %t12, align 1
  %v35 = trunc i8 %v34 to i1
  br i1 %v35, label %b2, label %b1
}

define hidden noundef i64 @tbaa.Box.hash(ptr noundef %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
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
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  %v5 = and i64 33, 63
  %v6 = lshr i64 %v4, %v5
  store i64 %v6, ptr %t3, align 8
  %v7 = load i64, ptr %t2, align 8
  %v8 = load i64, ptr %t3, align 8
  %v9 = xor i64 %v7, %v8
  store i64 %v9, ptr %t4, align 8
  %v10 = load i64, ptr %t4, align 8
  %v11 = mul i64 %v10, -49064778989728563
  store i64 %v11, ptr %t5, align 8
  %v12 = load i64, ptr %t5, align 8
  %v13 = and i64 33, 63
  %v14 = lshr i64 %v12, %v13
  store i64 %v14, ptr %t6, align 8
  %v15 = load i64, ptr %t5, align 8
  %v16 = load i64, ptr %t6, align 8
  %v17 = xor i64 %v15, %v16
  store i64 %v17, ptr %t7, align 8
  %v18 = load i64, ptr %t7, align 8
  %v19 = mul i64 %v18, -4265267296055464877
  store i64 %v19, ptr %t8, align 8
  %v20 = load i64, ptr %t8, align 8
  %v21 = and i64 33, 63
  %v22 = lshr i64 %v20, %v21
  store i64 %v22, ptr %t9, align 8
  %v23 = load i64, ptr %t8, align 8
  %v24 = load i64, ptr %t9, align 8
  %v25 = xor i64 %v23, %v24
  store i64 %v25, ptr %t10, align 8
  %v26 = load i64, ptr %t10, align 8
  %v27 = xor i64 %v26, -3750763034362895579
  store i64 %v27, ptr %t11, align 8
  %v28 = load i64, ptr %t11, align 8
  %v29 = and i64 33, 63
  %v30 = lshr i64 %v28, %v29
  store i64 %v30, ptr %t12, align 8
  %v31 = load i64, ptr %t11, align 8
  %v32 = load i64, ptr %t12, align 8
  %v33 = xor i64 %v31, %v32
  store i64 %v33, ptr %t13, align 8
  %v34 = load i64, ptr %t13, align 8
  %v35 = mul i64 %v34, -49064778989728563
  store i64 %v35, ptr %t14, align 8
  %v36 = load i64, ptr %t14, align 8
  %v37 = and i64 33, 63
  %v38 = lshr i64 %v36, %v37
  store i64 %v38, ptr %t15, align 8
  %v39 = load i64, ptr %t14, align 8
  %v40 = load i64, ptr %t15, align 8
  %v41 = xor i64 %v39, %v40
  store i64 %v41, ptr %t16, align 8
  %v42 = load i64, ptr %t16, align 8
  %v43 = mul i64 %v42, -4265267296055464877
  store i64 %v43, ptr %t17, align 8
  %v44 = load i64, ptr %t17, align 8
  %v45 = and i64 33, 63
  %v46 = lshr i64 %v44, %v45
  store i64 %v46, ptr %t18, align 8
  %v47 = load i64, ptr %t17, align 8
  %v48 = load i64, ptr %t18, align 8
  %v49 = xor i64 %v47, %v48
  store i64 %v49, ptr %t19, align 8
  %v50 = load i64, ptr %t19, align 8
  ret i64 %v50
}

define hidden void @tbaa.Box.destroy(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v0, i64 1)
  ret void
}

define hidden void @tbaa.Box.copy(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v0, ptr %v1, i64 16, i1 false)
  ret void
}

define hidden void @tbaa.Box.init(ptr noundef %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr @tbaa.Box.table, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  store i64 0, ptr %v4, align 8
  %v5 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v5, i64 0)
  ret void
}

define hidden noundef i8 @tbaa.Big.equals(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t5 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i8, align 1
  %t8 = alloca ptr, align 8
  %t9 = alloca ptr, align 8
  %t10 = alloca i32, align 4
  %t11 = alloca i32, align 4
  %t12 = alloca i8, align 1
  %t13 = alloca ptr, align 8
  %t14 = alloca ptr, align 8
  %t15 = alloca i64, align 8
  %t16 = alloca i64, align 8
  %t17 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store i8 1, ptr %t2, align 1
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %t1, align 8
  %v2 = icmp eq ptr %v0, %v1
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t3, align 1
  %v4 = load i8, ptr %t3, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b2, label %b3

b1:
  store i8 0, ptr %t2, align 1
  br label %b2

b2:
  %v6 = load i8, ptr %t2, align 1
  ret i8 %v6

b3:
  %v7 = load ptr, ptr %t1, align 8
  %v8 = icmp ne ptr %v7, null
  %v9 = zext i1 %v8 to i8
  store i8 %v9, ptr %t4, align 1
  %v10 = load i8, ptr %t4, align 1
  %v11 = trunc i8 %v10 to i1
  br i1 %v11, label %b4, label %b1

b4:
  %v12 = load ptr, ptr %t0, align 8
  %v13 = call ptr (ptr) @anti_rt_descriptor(ptr %v12)
  store ptr %v13, ptr %t5, align 8
  %v14 = load ptr, ptr %t1, align 8
  %v15 = call ptr (ptr) @anti_rt_descriptor(ptr %v14)
  store ptr %v15, ptr %t6, align 8
  %v16 = load ptr, ptr %t5, align 8
  %v17 = load ptr, ptr %t6, align 8
  %v18 = icmp eq ptr %v16, %v17
  %v19 = zext i1 %v18 to i8
  store i8 %v19, ptr %t7, align 1
  %v20 = load i8, ptr %t7, align 1
  %v21 = trunc i8 %v20 to i1
  br i1 %v21, label %b5, label %b1

b5:
  %v22 = load ptr, ptr %t0, align 8
  %v23 = getelementptr i8, ptr %v22, i64 16
  store ptr %v23, ptr %t8, align 8
  %v24 = load ptr, ptr %t1, align 8
  %v25 = getelementptr i8, ptr %v24, i64 16
  store ptr %v25, ptr %t9, align 8
  %v26 = load ptr, ptr %t8, align 8
  %v27 = load i32, ptr %v26, align 4
  store i32 %v27, ptr %t10, align 4
  %v28 = load ptr, ptr %t9, align 8
  %v29 = load i32, ptr %v28, align 4
  store i32 %v29, ptr %t11, align 4
  %v30 = load i32, ptr %t10, align 4
  %v31 = load i32, ptr %t11, align 4
  %v32 = icmp eq i32 %v30, %v31
  %v33 = zext i1 %v32 to i8
  store i8 %v33, ptr %t12, align 1
  %v34 = load i8, ptr %t12, align 1
  %v35 = trunc i8 %v34 to i1
  br i1 %v35, label %b6, label %b1

b6:
  %v36 = load ptr, ptr %t0, align 8
  %v37 = getelementptr i8, ptr %v36, i64 8
  store ptr %v37, ptr %t13, align 8
  %v38 = load ptr, ptr %t1, align 8
  %v39 = getelementptr i8, ptr %v38, i64 8
  store ptr %v39, ptr %t14, align 8
  %v40 = load ptr, ptr %t13, align 8
  %v41 = load i64, ptr %v40, align 8
  store i64 %v41, ptr %t15, align 8
  %v42 = load ptr, ptr %t14, align 8
  %v43 = load i64, ptr %v42, align 8
  store i64 %v43, ptr %t16, align 8
  %v44 = load i64, ptr %t15, align 8
  %v45 = load i64, ptr %t16, align 8
  %v46 = icmp eq i64 %v44, %v45
  %v47 = zext i1 %v46 to i8
  store i8 %v47, ptr %t17, align 1
  %v48 = load i8, ptr %t17, align 1
  %v49 = trunc i8 %v48 to i1
  br i1 %v49, label %b2, label %b1
}

define hidden noundef i64 @tbaa.Big.hash(ptr noundef %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i32, align 4
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
  %t21 = alloca ptr, align 8
  %t22 = alloca i64, align 8
  %t23 = alloca i64, align 8
  %t24 = alloca i64, align 8
  %t25 = alloca i64, align 8
  %t26 = alloca i64, align 8
  %t27 = alloca i64, align 8
  %t28 = alloca i64, align 8
  %t29 = alloca i64, align 8
  %t30 = alloca i64, align 8
  %t31 = alloca i64, align 8
  %t32 = alloca i64, align 8
  %t33 = alloca i64, align 8
  %t34 = alloca i64, align 8
  %t35 = alloca i64, align 8
  %t36 = alloca i64, align 8
  %t37 = alloca i64, align 8
  %t38 = alloca i64, align 8
  %t39 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr i8, ptr %v0, i64 16
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i32, ptr %v2, align 4
  store i32 %v3, ptr %t2, align 4
  %v4 = load i32, ptr %t2, align 4
  %v5 = sext i32 %v4 to i64
  store i64 %v5, ptr %t3, align 8
  %v6 = load i64, ptr %t3, align 8
  %v7 = and i64 33, 63
  %v8 = lshr i64 %v6, %v7
  store i64 %v8, ptr %t4, align 8
  %v9 = load i64, ptr %t3, align 8
  %v10 = load i64, ptr %t4, align 8
  %v11 = xor i64 %v9, %v10
  store i64 %v11, ptr %t5, align 8
  %v12 = load i64, ptr %t5, align 8
  %v13 = mul i64 %v12, -49064778989728563
  store i64 %v13, ptr %t6, align 8
  %v14 = load i64, ptr %t6, align 8
  %v15 = and i64 33, 63
  %v16 = lshr i64 %v14, %v15
  store i64 %v16, ptr %t7, align 8
  %v17 = load i64, ptr %t6, align 8
  %v18 = load i64, ptr %t7, align 8
  %v19 = xor i64 %v17, %v18
  store i64 %v19, ptr %t8, align 8
  %v20 = load i64, ptr %t8, align 8
  %v21 = mul i64 %v20, -4265267296055464877
  store i64 %v21, ptr %t9, align 8
  %v22 = load i64, ptr %t9, align 8
  %v23 = and i64 33, 63
  %v24 = lshr i64 %v22, %v23
  store i64 %v24, ptr %t10, align 8
  %v25 = load i64, ptr %t9, align 8
  %v26 = load i64, ptr %t10, align 8
  %v27 = xor i64 %v25, %v26
  store i64 %v27, ptr %t11, align 8
  %v28 = load i64, ptr %t11, align 8
  %v29 = xor i64 %v28, -3750763034362895579
  store i64 %v29, ptr %t12, align 8
  %v30 = load i64, ptr %t12, align 8
  %v31 = and i64 33, 63
  %v32 = lshr i64 %v30, %v31
  store i64 %v32, ptr %t13, align 8
  %v33 = load i64, ptr %t12, align 8
  %v34 = load i64, ptr %t13, align 8
  %v35 = xor i64 %v33, %v34
  store i64 %v35, ptr %t14, align 8
  %v36 = load i64, ptr %t14, align 8
  %v37 = mul i64 %v36, -49064778989728563
  store i64 %v37, ptr %t15, align 8
  %v38 = load i64, ptr %t15, align 8
  %v39 = and i64 33, 63
  %v40 = lshr i64 %v38, %v39
  store i64 %v40, ptr %t16, align 8
  %v41 = load i64, ptr %t15, align 8
  %v42 = load i64, ptr %t16, align 8
  %v43 = xor i64 %v41, %v42
  store i64 %v43, ptr %t17, align 8
  %v44 = load i64, ptr %t17, align 8
  %v45 = mul i64 %v44, -4265267296055464877
  store i64 %v45, ptr %t18, align 8
  %v46 = load i64, ptr %t18, align 8
  %v47 = and i64 33, 63
  %v48 = lshr i64 %v46, %v47
  store i64 %v48, ptr %t19, align 8
  %v49 = load i64, ptr %t18, align 8
  %v50 = load i64, ptr %t19, align 8
  %v51 = xor i64 %v49, %v50
  store i64 %v51, ptr %t20, align 8
  %v52 = load ptr, ptr %t0, align 8
  %v53 = getelementptr i8, ptr %v52, i64 8
  store ptr %v53, ptr %t21, align 8
  %v54 = load ptr, ptr %t21, align 8
  %v55 = load i64, ptr %v54, align 8
  store i64 %v55, ptr %t22, align 8
  %v56 = load i64, ptr %t22, align 8
  %v57 = and i64 33, 63
  %v58 = lshr i64 %v56, %v57
  store i64 %v58, ptr %t23, align 8
  %v59 = load i64, ptr %t22, align 8
  %v60 = load i64, ptr %t23, align 8
  %v61 = xor i64 %v59, %v60
  store i64 %v61, ptr %t24, align 8
  %v62 = load i64, ptr %t24, align 8
  %v63 = mul i64 %v62, -49064778989728563
  store i64 %v63, ptr %t25, align 8
  %v64 = load i64, ptr %t25, align 8
  %v65 = and i64 33, 63
  %v66 = lshr i64 %v64, %v65
  store i64 %v66, ptr %t26, align 8
  %v67 = load i64, ptr %t25, align 8
  %v68 = load i64, ptr %t26, align 8
  %v69 = xor i64 %v67, %v68
  store i64 %v69, ptr %t27, align 8
  %v70 = load i64, ptr %t27, align 8
  %v71 = mul i64 %v70, -4265267296055464877
  store i64 %v71, ptr %t28, align 8
  %v72 = load i64, ptr %t28, align 8
  %v73 = and i64 33, 63
  %v74 = lshr i64 %v72, %v73
  store i64 %v74, ptr %t29, align 8
  %v75 = load i64, ptr %t28, align 8
  %v76 = load i64, ptr %t29, align 8
  %v77 = xor i64 %v75, %v76
  store i64 %v77, ptr %t30, align 8
  %v78 = load i64, ptr %t20, align 8
  %v79 = load i64, ptr %t30, align 8
  %v80 = xor i64 %v78, %v79
  store i64 %v80, ptr %t31, align 8
  %v81 = load i64, ptr %t31, align 8
  %v82 = and i64 33, 63
  %v83 = lshr i64 %v81, %v82
  store i64 %v83, ptr %t32, align 8
  %v84 = load i64, ptr %t31, align 8
  %v85 = load i64, ptr %t32, align 8
  %v86 = xor i64 %v84, %v85
  store i64 %v86, ptr %t33, align 8
  %v87 = load i64, ptr %t33, align 8
  %v88 = mul i64 %v87, -49064778989728563
  store i64 %v88, ptr %t34, align 8
  %v89 = load i64, ptr %t34, align 8
  %v90 = and i64 33, 63
  %v91 = lshr i64 %v89, %v90
  store i64 %v91, ptr %t35, align 8
  %v92 = load i64, ptr %t34, align 8
  %v93 = load i64, ptr %t35, align 8
  %v94 = xor i64 %v92, %v93
  store i64 %v94, ptr %t36, align 8
  %v95 = load i64, ptr %t36, align 8
  %v96 = mul i64 %v95, -4265267296055464877
  store i64 %v96, ptr %t37, align 8
  %v97 = load i64, ptr %t37, align 8
  %v98 = and i64 33, 63
  %v99 = lshr i64 %v97, %v98
  store i64 %v99, ptr %t38, align 8
  %v100 = load i64, ptr %t37, align 8
  %v101 = load i64, ptr %t38, align 8
  %v102 = xor i64 %v100, %v101
  store i64 %v102, ptr %t39, align 8
  %v103 = load i64, ptr %t39, align 8
  ret i64 %v103
}

define hidden void @tbaa.Big.destroy(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v0, i64 1)
  ret void
}

define hidden void @tbaa.Big.copy(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v0, ptr %v1, i64 24, i1 false)
  ret void
}

define hidden void @tbaa.Big.init(ptr noundef %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr @tbaa.Big.table, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = load ptr, ptr %t0, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr i8, ptr %v2, i64 16
  store ptr %v3, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  store i32 0, ptr %v4, align 4
  %v5 = load ptr, ptr %t0, align 8
  %v6 = getelementptr i8, ptr %v5, i64 8
  store ptr %v6, ptr %t3, align 8
  %v7 = load ptr, ptr %t3, align 8
  store i64 0, ptr %v7, align 8
  %v8 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v8, i64 0)
  ret void
}

@tbaa.Box.table = hidden constant <{ ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }> <{ ptr @tbaa.Box.descriptor, ptr @anti_lang_Object_type_name, ptr @anti_lang_Object_to_text, ptr @tbaa.Box.equals, ptr @tbaa.Box.hash, ptr @anti_lang_Object_serialize, ptr @tbaa.Box.destroy, ptr @tbaa.Box.copy, ptr @anti_lang_Object_created, ptr @anti_lang_Object_destroyed, ptr @anti_lang_Object_copied, ptr @anti_lang_Object_dispatched, ptr @anti_lang_Object_joined, ptr @anti_lang_Object_enter, ptr @anti_lang_Object_leave, ptr @anti_lang_Object_failed, ptr @anti_lang_Object_changed }>, align 8
@tbaa.Box.descriptor = hidden constant <{ ptr, [8 x i8], ptr, [16 x i8], ptr, [8 x i8], ptr, [24 x i8], ptr, ptr, [32 x i8] }> <{ ptr @tbaa.2, [8 x i8] c"\03\00\00\00\00\00\00\00", ptr @anti_lang_Object_descriptor, [16 x i8] c"\10\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.Box.ancestors, [8 x i8] c"\01\00\00\00\00\00\00\00", ptr @tbaa.Box.fields, [24 x i8] c"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00", ptr @tbaa.Box.functions, ptr @tbaa.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.2 = hidden constant <{ [4 x i8] }> <{ [4 x i8] c"Box\00" }>, align 1
@anti_lang_Object_descriptor = external global [0 x i8], align 1
@tbaa.Box.ancestors = hidden constant <{ ptr, ptr }> <{ ptr @anti_lang_Object_descriptor, ptr @tbaa.Box.descriptor }>, align 8
@tbaa.4 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"n\00" }>, align 1
@tbaa.Box.fields = hidden constant <{ ptr, [40 x i8] }> <{ ptr @tbaa.4, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.6 = hidden constant <{ [4 x i8] }> <{ [4 x i8] c"str\00" }>, align 1
@tbaa.7 = hidden constant <{ [10 x i8] }> <{ [10 x i8] c"type_name\00" }>, align 1
@tbaa.8 = hidden constant <{ [8 x i8] }> <{ [8 x i8] c"to_text\00" }>, align 1
@tbaa.9 = hidden constant <{ [9 x i8] }> <{ [9 x i8] c"bool.ptr\00" }>, align 1
@tbaa.10 = hidden constant <{ [7 x i8] }> <{ [7 x i8] c"equals\00" }>, align 1
@tbaa.11 = hidden constant <{ [4 x i8] }> <{ [4 x i8] c"u64\00" }>, align 1
@tbaa.12 = hidden constant <{ [5 x i8] }> <{ [5 x i8] c"hash\00" }>, align 1
@tbaa.13 = hidden constant <{ [9 x i8] }> <{ [9 x i8] c"void.ptr\00" }>, align 1
@tbaa.14 = hidden constant <{ [10 x i8] }> <{ [10 x i8] c"serialize\00" }>, align 1
@tbaa.15 = hidden constant <{ [5 x i8] }> <{ [5 x i8] c"void\00" }>, align 1
@tbaa.16 = hidden constant <{ [9 x i8] }> <{ [9 x i8] c"destruct\00" }>, align 1
@tbaa.17 = hidden constant <{ [5 x i8] }> <{ [5 x i8] c"copy\00" }>, align 1
@tbaa.18 = hidden constant <{ [8 x i8] }> <{ [8 x i8] c"created\00" }>, align 1
@tbaa.19 = hidden constant <{ [10 x i8] }> <{ [10 x i8] c"destroyed\00" }>, align 1
@tbaa.20 = hidden constant <{ [7 x i8] }> <{ [7 x i8] c"copied\00" }>, align 1
@tbaa.21 = hidden constant <{ [11 x i8] }> <{ [11 x i8] c"dispatched\00" }>, align 1
@tbaa.22 = hidden constant <{ [7 x i8] }> <{ [7 x i8] c"joined\00" }>, align 1
@tbaa.23 = hidden constant <{ [9 x i8] }> <{ [9 x i8] c"void.str\00" }>, align 1
@tbaa.24 = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"enter\00" }>, align 1
@tbaa.25 = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"leave\00" }>, align 1
@tbaa.26 = hidden constant <{ [13 x i8] }> <{ [13 x i8] c"void.str.ptr\00" }>, align 1
@tbaa.27 = hidden constant <{ [7 x i8] }> <{ [7 x i8] c"failed\00" }>, align 1
@tbaa.28 = hidden constant <{ [8 x i8] }> <{ [8 x i8] c"changed\00" }>, align 1
@tbaa.Box.functions = hidden constant <{ ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr }> <{ ptr @tbaa.7, [24 x i8] c"\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.6, ptr @tbaa.8, [24 x i8] c"\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.6, ptr @tbaa.10, [24 x i8] c"\06\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.9, ptr @tbaa.12, [24 x i8] c"\04\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.11, ptr @tbaa.14, [24 x i8] c"\09\00\00\00\00\00\00\00\05\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.16, [24 x i8] c"\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.17, [24 x i8] c"\04\00\00\00\00\00\00\00\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.18, [24 x i8] c"\07\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.19, [24 x i8] c"\09\00\00\00\00\00\00\00\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.20, [24 x i8] c"\06\00\00\00\00\00\00\00\0A\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.21, [24 x i8] c"\0A\00\00\00\00\00\00\00\0B\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.22, [24 x i8] c"\06\00\00\00\00\00\00\00\0C\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.24, [24 x i8] c"\05\00\00\00\00\00\00\00\0D\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.23, ptr @tbaa.25, [24 x i8] c"\05\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.23, ptr @tbaa.27, [24 x i8] c"\06\00\00\00\00\00\00\00\0F\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00", ptr @tbaa.26, ptr @tbaa.28, [24 x i8] c"\07\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13 }>, align 8
@tbaa.package.version = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@tbaa.Big.table = hidden constant <{ ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }> <{ ptr @tbaa.Big.descriptor, ptr @anti_lang_Object_type_name, ptr @anti_lang_Object_to_text, ptr @tbaa.Big.equals, ptr @tbaa.Big.hash, ptr @anti_lang_Object_serialize, ptr @tbaa.Big.destroy, ptr @tbaa.Big.copy, ptr @anti_lang_Object_created, ptr @anti_lang_Object_destroyed, ptr @anti_lang_Object_copied, ptr @anti_lang_Object_dispatched, ptr @anti_lang_Object_joined, ptr @anti_lang_Object_enter, ptr @anti_lang_Object_leave, ptr @anti_lang_Object_failed, ptr @anti_lang_Object_changed }>, align 8
@tbaa.Big.descriptor = hidden constant <{ ptr, [8 x i8], ptr, [16 x i8], ptr, [8 x i8], ptr, [24 x i8], ptr, ptr, [32 x i8] }> <{ ptr @tbaa.33, [8 x i8] c"\03\00\00\00\00\00\00\00", ptr @tbaa.Box.descriptor, [16 x i8] c"\18\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.Big.ancestors, [8 x i8] c"\01\00\00\00\00\00\00\00", ptr @tbaa.Big.fields, [24 x i8] c"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00", ptr @tbaa.Big.functions, ptr @tbaa.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.33 = hidden constant <{ [4 x i8] }> <{ [4 x i8] c"Big\00" }>, align 1
@tbaa.Big.ancestors = hidden constant <{ ptr, ptr, ptr }> <{ ptr @anti_lang_Object_descriptor, ptr @tbaa.Box.descriptor, ptr @tbaa.Big.descriptor }>, align 8
@tbaa.35 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"m\00" }>, align 1
@tbaa.Big.fields = hidden constant <{ ptr, [40 x i8] }> <{ ptr @tbaa.35, [40 x i8] c"\01\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.Big.functions = hidden constant <{ ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr }> <{ ptr @tbaa.7, [24 x i8] c"\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.6, ptr @tbaa.8, [24 x i8] c"\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.6, ptr @tbaa.10, [24 x i8] c"\06\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.9, ptr @tbaa.12, [24 x i8] c"\04\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.11, ptr @tbaa.14, [24 x i8] c"\09\00\00\00\00\00\00\00\05\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.16, [24 x i8] c"\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.17, [24 x i8] c"\04\00\00\00\00\00\00\00\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.18, [24 x i8] c"\07\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.19, [24 x i8] c"\09\00\00\00\00\00\00\00\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.20, [24 x i8] c"\06\00\00\00\00\00\00\00\0A\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13, ptr @tbaa.21, [24 x i8] c"\0A\00\00\00\00\00\00\00\0B\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.22, [24 x i8] c"\06\00\00\00\00\00\00\00\0C\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tbaa.15, ptr @tbaa.24, [24 x i8] c"\05\00\00\00\00\00\00\00\0D\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.23, ptr @tbaa.25, [24 x i8] c"\05\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.23, ptr @tbaa.27, [24 x i8] c"\06\00\00\00\00\00\00\00\0F\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00", ptr @tbaa.26, ptr @tbaa.28, [24 x i8] c"\07\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tbaa.13 }>, align 8
@tbaa.Point.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @tbaa.39, [48 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\18\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00", ptr @tbaa.Point.fields, [32 x i8] zeroinitializer, ptr @tbaa.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.39 = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"Point\00" }>, align 1
@tbaa.40 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"x\00" }>, align 1
@tbaa.41 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"y\00" }>, align 1
@tbaa.42 = hidden constant <{ [3 x i8] }> <{ [3 x i8] c"on\00" }>, align 1
@tbaa.Point.fields = hidden constant <{ ptr, [40 x i8], ptr, [40 x i8], ptr, [40 x i8] }> <{ ptr @tbaa.40, [40 x i8] c"\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @tbaa.41, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\0F\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @tbaa.42, [40 x i8] c"\02\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.V4.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @tbaa.45, [48 x i8] c"\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00", ptr @tbaa.V4.fields, [32 x i8] zeroinitializer, ptr @tbaa.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.45 = hidden constant <{ [3 x i8] }> <{ [3 x i8] c"V4\00" }>, align 1
@tbaa.46 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"z\00" }>, align 1
@tbaa.47 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"w\00" }>, align 1
@tbaa.V4.fields = hidden constant <{ ptr, [40 x i8], ptr, [40 x i8], ptr, [40 x i8], ptr, [40 x i8] }> <{ ptr @tbaa.40, [40 x i8] c"\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @tbaa.41, [40 x i8] c"\01\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @tbaa.46, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @tbaa.47, [40 x i8] c"\01\00\00\00\00\00\00\00\0C\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tbaa.49 = hidden constant <{ [34 x i8] }> <{ [34 x i8] c"tbaa.anti:40: index out of bounds\00" }>, align 1

declare [2 x i64] @anti_lang_Object_type_name(ptr noundef nonnull dereferenceable(8)) #1
declare [2 x i64] @anti_lang_Object_to_text(ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_serialize(ptr noundef nonnull dereferenceable(8), ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_created(ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_destroyed(ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_copied(ptr noundef nonnull dereferenceable(8), ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_dispatched(ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_joined(ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_enter(ptr noundef nonnull dereferenceable(8), [2 x i64]) #1
declare void @anti_lang_Object_leave(ptr noundef nonnull dereferenceable(8), [2 x i64]) #1
declare void @anti_lang_Object_failed(ptr noundef nonnull dereferenceable(8), [2 x i64], ptr noundef nonnull dereferenceable(8)) #1
declare void @anti_lang_Object_changed(ptr noundef nonnull dereferenceable(8), ptr noundef nonnull dereferenceable(48)) #1
declare void @anti_rt_hook(ptr noundef, i64 noundef) #1
declare noundef ptr @anti_rt_descriptor(ptr noundef) #1
declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!4 = !{!"branch_weights", i32 2000, i32 1}
!5 = !{i8 0, i8 2}
!6 = !{!"anti"}
!7 = !{!"byte", !6, i64 0}
!8 = !{!"i16", !7, i64 0}
!9 = !{!"i32", !7, i64 0}
!10 = !{!"i64", !7, i64 0}
!11 = !{!"f32", !7, i64 0}
!12 = !{!"f64", !7, i64 0}
!13 = !{!"ptr", !7, i64 0}
!14 = !{!8, !8, i64 0}
!15 = !{!9, !9, i64 0}
!16 = !{!10, !10, i64 0}
!17 = !{!11, !11, i64 0}
!18 = !{!12, !12, i64 0}
!19 = !{!13, !13, i64 0}
!20 = !{!"tbaa.Point", !10, i64 0, !12, i64 8}
!21 = !{!"tbaa.Big", !9, i64 16}
!22 = !{!"tbaa.Box", !10, i64 8}
!23 = !{!20, !10, i64 0}
!24 = !{!20, !12, i64 8}
!25 = !{!21, !9, i64 16}
!26 = !{!22, !10, i64 8}
