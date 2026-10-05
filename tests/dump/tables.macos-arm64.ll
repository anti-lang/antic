source_filename = "tables"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i64 @tables.Circle.area(ptr noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8, !tbaa !21
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  %v5 = mul i64 %v4, 10
  store i64 %v5, ptr %t3, align 8
  %v6 = load i64, ptr %t3, align 8
  ret i64 %v6
}

define internal noundef i64 @tables.measure(ptr noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %v0, align 8, !invariant.group !5
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = getelementptr i8, ptr %v2, i64 136
  store ptr %v3, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  %v5 = load ptr, ptr %v4, align 8, !invariant.load !5
  store ptr %v5, ptr %t3, align 8
  %v6 = load ptr, ptr %t3, align 8
  %v7 = load ptr, ptr %t0, align 8
  %v8 = call i64 (ptr) %v6(ptr %v7)
  store i64 %v8, ptr %t4, align 8
  %v9 = load i64, ptr %t4, align 8
  ret i64 %v9
}

define internal noundef i64 @tables.main() #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i8, align 1
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  %v0 = call ptr (i64) @malloc(i64 16)
  store ptr %v0, ptr %t0, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = icmp eq ptr %v1, null
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t1, align 1
  %v4 = load i8, ptr %t1, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b1, label %b2

b1:
  call void (i64) @anti_rt_out_of_memory(i64 16)
  unreachable

b2:
  store ptr @tables.Circle.table, ptr %t2, align 8
  %v6 = load ptr, ptr %t2, align 8
  %v7 = load ptr, ptr %t0, align 8
  store ptr %v6, ptr %v7, align 8, !invariant.group !5
  %v8 = load ptr, ptr %t0, align 8
  %v9 = getelementptr i8, ptr %v8, i64 8
  store ptr %v9, ptr %t3, align 8
  %v10 = load ptr, ptr %t3, align 8
  store i64 1, ptr %v10, align 8
  %v11 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v11, i64 0)
  %v12 = load ptr, ptr %t0, align 8
  %v13 = load ptr, ptr %v12, align 8, !invariant.group !5
  store ptr %v13, ptr %t4, align 8
  %v14 = load ptr, ptr %t4, align 8
  %v15 = getelementptr i8, ptr %v14, i64 136
  store ptr %v15, ptr %t5, align 8
  %v16 = load ptr, ptr %t5, align 8
  %v17 = load ptr, ptr %v16, align 8, !invariant.load !5
  store ptr %v17, ptr %t6, align 8
  %v18 = load ptr, ptr %t6, align 8
  %v19 = load ptr, ptr %t0, align 8
  %v20 = call i64 (ptr) %v18(ptr %v19)
  store i64 %v20, ptr %t7, align 8
  %v21 = load ptr, ptr %t0, align 8
  %v22 = call i64 (ptr) @tables.measure(ptr %v21)
  store i64 %v22, ptr %t8, align 8
  %v23 = load i64, ptr %t7, align 8
  %v24 = load i64, ptr %t8, align 8
  %v25 = add i64 %v23, %v24
  store i64 %v25, ptr %t9, align 8
  store ptr @tables.Shape.descriptor, ptr %t10, align 8
  %v26 = load ptr, ptr %t0, align 8
  %v27 = load ptr, ptr %t10, align 8
  call void (ptr, ptr) @anti_rt_delete(ptr %v26, ptr %v27)
  %v28 = load i64, ptr %t9, align 8
  ret i64 %v28
}

define internal noundef i8 @tables.Circle.equals(ptr noundef %p0, ptr noundef %p1) #0 {
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

define internal noundef i64 @tables.Circle.hash(ptr noundef %p0) #0 {
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

define internal void @tables.Circle.destroy(ptr noundef %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8
  call void (ptr, i64) @anti_rt_hook(ptr %v0, i64 1)
  ret void
}

define internal void @tables.Circle.copy(ptr noundef %p0, ptr noundef %p1) #0 {
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

@anti.rt.main = alias i64 (), ptr @tables.main

@tables.Shape.descriptor = internal constant <{ ptr, [8 x i8], ptr, [16 x i8], ptr, [8 x i8], ptr, [24 x i8], ptr, ptr, [32 x i8] }> <{ ptr @tables.2, [8 x i8] c"\05\00\00\00\00\00\00\00", ptr @anti_lang_Object_descriptor, [16 x i8] c"\10\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.Shape.ancestors, [8 x i8] c"\01\00\00\00\00\00\00\00", ptr @tables.Shape.fields, [24 x i8] c"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\11\00\00\00\00\00\00\00", ptr @tables.Shape.functions, ptr @tables.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tables.2 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"Shape\00" }>, align 1
@anti_lang_Object_descriptor = external global [0 x i8], align 1
@tables.Shape.ancestors = internal constant <{ ptr, ptr }> <{ ptr @anti_lang_Object_descriptor, ptr @tables.Shape.descriptor }>, align 8
@tables.4 = internal constant <{ [2 x i8] }> <{ [2 x i8] c"n\00" }>, align 1
@tables.Shape.fields = internal constant <{ ptr, [40 x i8] }> <{ ptr @tables.4, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tables.6 = internal constant <{ [4 x i8] }> <{ [4 x i8] c"str\00" }>, align 1
@tables.7 = internal constant <{ [10 x i8] }> <{ [10 x i8] c"type_name\00" }>, align 1
@tables.8 = internal constant <{ [8 x i8] }> <{ [8 x i8] c"to_text\00" }>, align 1
@tables.9 = internal constant <{ [9 x i8] }> <{ [9 x i8] c"bool.ptr\00" }>, align 1
@tables.10 = internal constant <{ [7 x i8] }> <{ [7 x i8] c"equals\00" }>, align 1
@tables.11 = internal constant <{ [4 x i8] }> <{ [4 x i8] c"u64\00" }>, align 1
@tables.12 = internal constant <{ [5 x i8] }> <{ [5 x i8] c"hash\00" }>, align 1
@tables.13 = internal constant <{ [9 x i8] }> <{ [9 x i8] c"void.ptr\00" }>, align 1
@tables.14 = internal constant <{ [10 x i8] }> <{ [10 x i8] c"serialize\00" }>, align 1
@tables.15 = internal constant <{ [5 x i8] }> <{ [5 x i8] c"void\00" }>, align 1
@tables.16 = internal constant <{ [9 x i8] }> <{ [9 x i8] c"destruct\00" }>, align 1
@tables.17 = internal constant <{ [5 x i8] }> <{ [5 x i8] c"copy\00" }>, align 1
@tables.18 = internal constant <{ [8 x i8] }> <{ [8 x i8] c"created\00" }>, align 1
@tables.19 = internal constant <{ [10 x i8] }> <{ [10 x i8] c"destroyed\00" }>, align 1
@tables.20 = internal constant <{ [7 x i8] }> <{ [7 x i8] c"copied\00" }>, align 1
@tables.21 = internal constant <{ [11 x i8] }> <{ [11 x i8] c"dispatched\00" }>, align 1
@tables.22 = internal constant <{ [7 x i8] }> <{ [7 x i8] c"joined\00" }>, align 1
@tables.23 = internal constant <{ [9 x i8] }> <{ [9 x i8] c"void.str\00" }>, align 1
@tables.24 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"enter\00" }>, align 1
@tables.25 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"leave\00" }>, align 1
@tables.26 = internal constant <{ [13 x i8] }> <{ [13 x i8] c"void.str.ptr\00" }>, align 1
@tables.27 = internal constant <{ [7 x i8] }> <{ [7 x i8] c"failed\00" }>, align 1
@tables.28 = internal constant <{ [8 x i8] }> <{ [8 x i8] c"changed\00" }>, align 1
@tables.29 = internal constant <{ [4 x i8] }> <{ [4 x i8] c"i64\00" }>, align 1
@tables.30 = internal constant <{ [5 x i8] }> <{ [5 x i8] c"area\00" }>, align 1
@tables.Shape.functions = internal constant <{ ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr }> <{ ptr @tables.7, [24 x i8] c"\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.6, ptr @tables.8, [24 x i8] c"\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.6, ptr @tables.10, [24 x i8] c"\06\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.9, ptr @tables.12, [24 x i8] c"\04\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.11, ptr @tables.14, [24 x i8] c"\09\00\00\00\00\00\00\00\05\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.16, [24 x i8] c"\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.17, [24 x i8] c"\04\00\00\00\00\00\00\00\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.18, [24 x i8] c"\07\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.19, [24 x i8] c"\09\00\00\00\00\00\00\00\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.20, [24 x i8] c"\06\00\00\00\00\00\00\00\0A\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.21, [24 x i8] c"\0A\00\00\00\00\00\00\00\0B\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.22, [24 x i8] c"\06\00\00\00\00\00\00\00\0C\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.24, [24 x i8] c"\05\00\00\00\00\00\00\00\0D\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.23, ptr @tables.25, [24 x i8] c"\05\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.23, ptr @tables.27, [24 x i8] c"\06\00\00\00\00\00\00\00\0F\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00", ptr @tables.26, ptr @tables.28, [24 x i8] c"\07\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.30, [24 x i8] c"\04\00\00\00\00\00\00\00\11\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.29 }>, align 8
@tables.package.version = internal constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@tables.Circle.table = internal constant <{ ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }> <{ ptr @tables.Circle.descriptor, ptr @anti_lang_Object_type_name, ptr @anti_lang_Object_to_text, ptr @tables.Circle.equals, ptr @tables.Circle.hash, ptr @anti_lang_Object_serialize, ptr @tables.Circle.destroy, ptr @tables.Circle.copy, ptr @anti_lang_Object_created, ptr @anti_lang_Object_destroyed, ptr @anti_lang_Object_copied, ptr @anti_lang_Object_dispatched, ptr @anti_lang_Object_joined, ptr @anti_lang_Object_enter, ptr @anti_lang_Object_leave, ptr @anti_lang_Object_failed, ptr @anti_lang_Object_changed, ptr @tables.Circle.area }>, align 8
@tables.Circle.descriptor = internal constant <{ ptr, [8 x i8], ptr, [16 x i8], ptr, [40 x i8], ptr, ptr, [32 x i8] }> <{ ptr @tables.35, [8 x i8] c"\06\00\00\00\00\00\00\00", ptr @tables.Shape.descriptor, [16 x i8] c"\10\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.Circle.ancestors, [40 x i8] c"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\11\00\00\00\00\00\00\00", ptr @tables.Circle.functions, ptr @tables.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@tables.35 = internal constant <{ [7 x i8] }> <{ [7 x i8] c"Circle\00" }>, align 1
@tables.Circle.ancestors = internal constant <{ ptr, ptr, ptr }> <{ ptr @anti_lang_Object_descriptor, ptr @tables.Shape.descriptor, ptr @tables.Circle.descriptor }>, align 8
@tables.Circle.functions = internal constant <{ ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr, ptr, [24 x i8], ptr }> <{ ptr @tables.7, [24 x i8] c"\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.6, ptr @tables.8, [24 x i8] c"\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.6, ptr @tables.10, [24 x i8] c"\06\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.9, ptr @tables.12, [24 x i8] c"\04\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.11, ptr @tables.14, [24 x i8] c"\09\00\00\00\00\00\00\00\05\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.16, [24 x i8] c"\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.17, [24 x i8] c"\04\00\00\00\00\00\00\00\07\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.18, [24 x i8] c"\07\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.19, [24 x i8] c"\09\00\00\00\00\00\00\00\09\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.20, [24 x i8] c"\06\00\00\00\00\00\00\00\0A\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.21, [24 x i8] c"\0A\00\00\00\00\00\00\00\0B\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.22, [24 x i8] c"\06\00\00\00\00\00\00\00\0C\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.15, ptr @tables.24, [24 x i8] c"\05\00\00\00\00\00\00\00\0D\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.23, ptr @tables.25, [24 x i8] c"\05\00\00\00\00\00\00\00\0E\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.23, ptr @tables.27, [24 x i8] c"\06\00\00\00\00\00\00\00\0F\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00", ptr @tables.26, ptr @tables.28, [24 x i8] c"\07\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @tables.13, ptr @tables.30, [24 x i8] c"\04\00\00\00\00\00\00\00\11\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @tables.29 }>, align 8
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

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
declare noundef i64 @tables.fn.0(ptr noundef nonnull dereferenceable(16)) #1
declare noundef ptr @malloc(i64 noundef) #1
declare void @anti_rt_out_of_memory(i64 noundef) noreturn cold #1
declare void @anti_rt_delete(ptr noundef, ptr noundef) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!5 = !{}
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
!20 = !{!"tables.Shape", !10, i64 8}
!21 = !{!20, !10, i64 8}
