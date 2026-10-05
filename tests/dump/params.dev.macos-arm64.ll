source_filename = "params"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define hidden noundef i64 @params.read(ptr noundef nonnull dereferenceable(16) %p0, ptr noundef %p1) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca ptr, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca ptr, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t1, align 8
  %v1 = icmp ne ptr %v0, null
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t2, align 1
  %v3 = load i8, ptr %t2, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b1, label %b2

b1:
  %v5 = load ptr, ptr %t0, align 8
  %v6 = getelementptr inbounds i8, ptr %v5, i64 8
  store ptr %v6, ptr %t3, align 8
  %v7 = load ptr, ptr %t3, align 8
  %v8 = load i64, ptr %v7, align 8
  store i64 %v8, ptr %t4, align 8
  %v9 = load ptr, ptr %t1, align 8
  %v10 = getelementptr inbounds i8, ptr %v9, i64 8
  store ptr %v10, ptr %t5, align 8
  %v11 = load ptr, ptr %t5, align 8
  %v12 = load i64, ptr %v11, align 8
  store i64 %v12, ptr %t6, align 8
  %v13 = load i64, ptr %t4, align 8
  %v14 = load i64, ptr %t6, align 8
  %v15 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %v13, i64 %v14)
  %v16 = add nsw i64 %v13, %v14
  %v17 = extractvalue { i64, i1 } %v15, 1
  store i64 %v16, ptr %t7, align 8
  br i1 %v17, label %b3, label %b4, !prof !3

b2:
  %v18 = load ptr, ptr %t0, align 8
  %v19 = getelementptr inbounds i8, ptr %v18, i64 8
  store ptr %v19, ptr %t8, align 8
  %v20 = load ptr, ptr %t8, align 8
  %v21 = load i64, ptr %v20, align 8
  store i64 %v21, ptr %t9, align 8
  %v22 = load i64, ptr %t9, align 8
  ret i64 %v22

b3:
  store ptr @params.6, ptr %t10, align 8
  %v23 = load ptr, ptr %t10, align 8
  %v24 = load i64, ptr %t4, align 8
  %v25 = load i64, ptr %t6, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v23, i64 29, i32 1, i64 %v24, i64 %v25)
  unreachable

b4:
  %v26 = load i64, ptr %t7, align 8
  ret i64 %v26
}

define hidden noundef i64 @params.take(ptr noalias noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  ret i64 %v4
}

define hidden noundef signext i8 @params.small(i8 signext noundef %p0) #0 {
b0:
  %t0 = alloca i8, align 1
  store i8 %p0, ptr %t0, align 1
  %v0 = load i8, ptr %t0, align 1
  ret i8 %v0
}

define hidden noundef zeroext i16 @params.wide(i16 zeroext noundef %p0) #0 {
b0:
  %t0 = alloca i16, align 2
  store i16 %p0, ptr %t0, align 2
  %v0 = load i16, ptr %t0, align 2
  ret i16 %v0
}

define hidden noundef zeroext i8 @params.first([2 x i64] %p0.0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i8, align 1
  %a0 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i8, ptr %v0, align 1
  store i8 %v1, ptr %t1, align 1
  %v2 = load i8, ptr %t1, align 1
  ret i8 %v2
}

define hidden noundef ptr @params.raw() #0 {
b0:
  %t0 = alloca ptr, align 8
  %v0 = call ptr (i64, i64) @anti_rt_mem_alloc(i64 16, i64 8)
  store ptr %v0, ptr %t0, align 8
  %v1 = load ptr, ptr %t0, align 8
  ret ptr %v1
}

@params.Gap.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @params.1, [48 x i8] c"\03\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @params.Gap.fields, [32 x i8] zeroinitializer, ptr @params.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@params.1 = hidden constant <{ [4 x i8] }> <{ [4 x i8] c"Gap\00" }>, align 1
@params.2 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"a\00" }>, align 1
@params.3 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"b\00" }>, align 1
@params.Gap.fields = hidden constant <{ ptr, [40 x i8], ptr, [40 x i8] }> <{ ptr @params.2, [40 x i8] c"\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @params.3, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@params.package.version = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@params.6 = hidden constant <{ [30 x i8] }> <{ [30 x i8] c"params.anti:19: overflow in +\00" }>, align 1

declare noalias noundef ptr @anti_rt_mem_alloc(i64 noundef, i64 noundef) #1
declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, i64)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
