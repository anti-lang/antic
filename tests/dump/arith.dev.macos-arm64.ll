source_filename = "arith"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define hidden noundef i64 @arith.checked(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca ptr, align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %v0, i64 %v1)
  %v3 = add nsw i64 %v0, %v1
  %v4 = extractvalue { i64, i1 } %v2, 1
  store i64 %v3, ptr %t2, align 8
  br i1 %v4, label %b1, label %b2, !prof !3

b1:
  store ptr @arith.11, ptr %t3, align 8
  %v5 = load ptr, ptr %t3, align 8
  %v6 = load i64, ptr %t0, align 8
  %v7 = load i64, ptr %t1, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v5, i64 28, i32 1, i64 %v6, i64 %v7)
  unreachable

b2:
  %v8 = load i64, ptr %t2, align 8
  ret i64 %v8
}

define hidden noundef i64 @arith.up(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i8, align 1
  %t4 = alloca i64, align 8
  %t5 = alloca i8, align 1
  store i64 %p0, ptr %t0, align 8
  store i64 0, ptr %t1, align 8
  store i64 0, ptr %t2, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = icmp slt i64 0, %v0
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t3, align 1
  %v3 = load i8, ptr %t3, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b1, label %b2

b1:
  %v5 = load i64, ptr %t1, align 8
  %v6 = load i64, ptr %t2, align 8
  %v7 = add i64 %v5, %v6
  store i64 %v7, ptr %t1, align 8
  %v8 = load i64, ptr %t0, align 8
  %v9 = load i64, ptr %t2, align 8
  %v10 = sub i64 %v8, %v9
  store i64 %v10, ptr %t4, align 8
  %v11 = load i64, ptr %t4, align 8
  %v12 = icmp ule i64 %v11, 1
  %v13 = zext i1 %v12 to i8
  store i8 %v13, ptr %t5, align 1
  %v14 = load i8, ptr %t5, align 1
  %v15 = trunc i8 %v14 to i1
  br i1 %v15, label %b2, label %b3

b2:
  %v16 = load i64, ptr %t1, align 8
  ret i64 %v16

b3:
  %v17 = load i64, ptr %t2, align 8
  %v18 = add nuw nsw i64 %v17, 1
  store i64 %v18, ptr %t2, align 8
  br label %b1
}

define hidden noundef i64 @arith.down(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i8, align 1
  %t5 = alloca i64, align 8
  %t6 = alloca i8, align 1
  %t7 = alloca i64, align 8
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store i64 0, ptr %t2, align 8
  %v0 = load i64, ptr %t0, align 8
  store i64 %v0, ptr %t3, align 8
  %v1 = load i64, ptr %t0, align 8
  %v2 = load i64, ptr %t1, align 8
  %v3 = icmp slt i64 %v1, %v2
  %v4 = zext i1 %v3 to i8
  store i8 %v4, ptr %t4, align 1
  %v5 = load i8, ptr %t4, align 1
  %v6 = trunc i8 %v5 to i1
  br i1 %v6, label %b3, label %b2

b1:
  %v7 = load i64, ptr %t2, align 8
  %v8 = load i64, ptr %t3, align 8
  %v9 = add i64 %v7, %v8
  store i64 %v9, ptr %t2, align 8
  %v10 = load i64, ptr %t3, align 8
  %v11 = load i64, ptr %t0, align 8
  %v12 = sub i64 %v10, %v11
  store i64 %v12, ptr %t5, align 8
  %v13 = load i64, ptr %t5, align 8
  %v14 = icmp ult i64 %v13, 2
  %v15 = zext i1 %v14 to i8
  store i8 %v15, ptr %t6, align 1
  %v16 = load i8, ptr %t6, align 1
  %v17 = trunc i8 %v16 to i1
  br i1 %v17, label %b2, label %b4

b2:
  %v18 = load i64, ptr %t2, align 8
  ret i64 %v18

b3:
  %v19 = load i64, ptr %t1, align 8
  %v20 = load i64, ptr %t0, align 8
  %v21 = sub i64 %v19, %v20
  store i64 %v21, ptr %t7, align 8
  %v22 = load i64, ptr %t7, align 8
  %v23 = sub i64 %v22, 1
  store i64 %v23, ptr %t8, align 8
  %v24 = load i64, ptr %t8, align 8
  %v25 = and i64 1, 63
  %v26 = lshr i64 %v24, %v25
  store i64 %v26, ptr %t9, align 8
  %v27 = load i64, ptr %t9, align 8
  %v28 = and i64 1, 63
  %v29 = shl i64 %v27, %v28
  store i64 %v29, ptr %t10, align 8
  %v30 = load i64, ptr %t0, align 8
  %v31 = load i64, ptr %t10, align 8
  %v32 = add i64 %v30, %v31
  store i64 %v32, ptr %t3, align 8
  br label %b1

b4:
  %v33 = load i64, ptr %t3, align 8
  %v34 = sub nsw i64 %v33, 2
  store i64 %v34, ptr %t3, align 8
  br label %b1
}

define hidden noundef zeroext i8 @arith.bytes(i8 zeroext noundef %p0, i8 zeroext noundef %p1) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t5 = alloca i8, align 1
  %t6 = alloca i8, align 1
  store i8 %p0, ptr %t0, align 1
  store i8 %p1, ptr %t1, align 1
  store i8 0, ptr %t2, align 1
  %v0 = load i8, ptr %t0, align 1
  store i8 %v0, ptr %t3, align 1
  %v1 = load i8, ptr %t0, align 1
  %v2 = load i8, ptr %t1, align 1
  %v3 = icmp ult i8 %v1, %v2
  %v4 = zext i1 %v3 to i8
  store i8 %v4, ptr %t4, align 1
  %v5 = load i8, ptr %t4, align 1
  %v6 = trunc i8 %v5 to i1
  br i1 %v6, label %b1, label %b2

b1:
  %v7 = load i8, ptr %t2, align 1
  %v8 = load i8, ptr %t3, align 1
  %v9 = add i8 %v7, %v8
  store i8 %v9, ptr %t2, align 1
  %v10 = load i8, ptr %t1, align 1
  %v11 = load i8, ptr %t3, align 1
  %v12 = sub i8 %v10, %v11
  store i8 %v12, ptr %t5, align 1
  %v13 = load i8, ptr %t5, align 1
  %v14 = icmp ule i8 %v13, 1
  %v15 = zext i1 %v14 to i8
  store i8 %v15, ptr %t6, align 1
  %v16 = load i8, ptr %t6, align 1
  %v17 = trunc i8 %v16 to i1
  br i1 %v17, label %b2, label %b3

b2:
  %v18 = load i8, ptr %t2, align 1
  ret i8 %v18

b3:
  %v19 = load i8, ptr %t3, align 1
  %v20 = add nuw i8 %v19, 1
  store i8 %v20, ptr %t3, align 1
  br label %b1
}

define hidden noundef i64 @arith.field(ptr noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8, !tbaa !23
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  ret i64 %v4
}

define hidden noundef i64 @arith.viewed(ptr noundef nonnull dereferenceable(16) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr i8, ptr %v0, i64 8
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i64, ptr %v2, align 8
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  ret i64 %v4
}

define hidden noundef range(i32 0, 3) i32 @arith.kind(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i32, align 4
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i32, ptr %v0, align 4, !range !5, !tbaa !24
  store i32 %v1, ptr %t1, align 4
  %v2 = load i32, ptr %t1, align 4
  ret i32 %v2
}

define hidden noundef zeroext range(i8 0, 2) i8 @arith.on(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = getelementptr inbounds i8, ptr %v0, i64 4
  store ptr %v1, ptr %t1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = load i8, ptr %v2, align 1, !range !6
  store i8 %v3, ptr %t2, align 1
  %v4 = load i8, ptr %t2, align 1
  ret i8 %v4
}

define hidden noundef zeroext range(i8 0, 2) i8 @arith.round([2 x i64] %p0.0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %a0 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i8, ptr %v0, align 1, !range !6
  store i8 %v1, ptr %t1, align 1
  %v2 = load i8, ptr %t1, align 1
  %v3 = icmp eq i8 %v2, 0
  %v4 = zext i1 %v3 to i8
  store i8 %v4, ptr %t2, align 1
  %v5 = load i8, ptr %t2, align 1
  ret i8 %v5
}

define hidden noundef zeroext range(i8 0, 2) i8 @arith.is_line(ptr noundef nonnull dereferenceable(8) %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i32, align 4
  %t2 = alloca i8, align 1
  store ptr %p0, ptr %t0, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = call range(i32 0, 3) i32 (ptr) @arith.kind(ptr %v0)
  store i32 %v1, ptr %t1, align 4
  %v2 = load i32, ptr %t1, align 4
  %v3 = icmp eq i32 %v2, 2
  %v4 = zext i1 %v3 to i8
  store i8 %v4, ptr %t2, align 1
  %v5 = load i8, ptr %t2, align 1
  ret i8 %v5
}

define hidden noundef i64 @arith.scoped(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i8, align 1
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i8, align 1
  %s1 = alloca [16 x i8], align 8
  store i64 %p0, ptr %t0, align 8
  store ptr %s1, ptr %t1, align 8
  store i64 0, ptr %t2, align 8
  store i64 0, ptr %t3, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = icmp slt i64 0, %v0
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t4, align 1
  %v3 = load i8, ptr %t4, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b1, label %b2

b1:
  call void @llvm.lifetime.start.p0(ptr %s1)
  %v5 = load i64, ptr %t3, align 8
  %v6 = load ptr, ptr %t1, align 8
  store i64 %v5, ptr %v6, align 8
  %v7 = load ptr, ptr %t1, align 8
  %v8 = getelementptr i8, ptr %v7, i64 8
  store ptr %v8, ptr %t5, align 8
  %v9 = load ptr, ptr %t5, align 8
  store i64 2, ptr %v9, align 8
  %v10 = load ptr, ptr %t1, align 8
  %v11 = call i64 (ptr) @arith.field(ptr %v10)
  store i64 %v11, ptr %t6, align 8
  %v12 = load i64, ptr %t2, align 8
  %v13 = load i64, ptr %t6, align 8
  %v14 = add i64 %v12, %v13
  store i64 %v14, ptr %t2, align 8
  call void @llvm.lifetime.end.p0(ptr %s1)
  %v15 = load i64, ptr %t0, align 8
  %v16 = load i64, ptr %t3, align 8
  %v17 = sub i64 %v15, %v16
  store i64 %v17, ptr %t7, align 8
  %v18 = load i64, ptr %t7, align 8
  %v19 = icmp ule i64 %v18, 1
  %v20 = zext i1 %v19 to i8
  store i8 %v20, ptr %t8, align 1
  %v21 = load i8, ptr %t8, align 1
  %v22 = trunc i8 %v21 to i1
  br i1 %v22, label %b2, label %b3

b2:
  %v23 = load i64, ptr %t2, align 8
  ret i64 %v23

b3:
  %v24 = load i64, ptr %t3, align 8
  %v25 = add nuw nsw i64 %v24, 1
  store i64 %v25, ptr %t3, align 8
  br label %b1
}

@arith.Point.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @arith.1, [48 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @arith.Point.fields, [32 x i8] zeroinitializer, ptr @arith.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@arith.1 = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"Point\00" }>, align 1
@arith.2 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"x\00" }>, align 1
@arith.3 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"y\00" }>, align 1
@arith.Point.fields = hidden constant <{ ptr, [40 x i8], ptr, [40 x i8] }> <{ ptr @arith.2, [40 x i8] c"\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @arith.3, [40 x i8] c"\01\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@arith.package.version = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@arith.Holder.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @arith.7, [48 x i8] c"\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00", ptr @arith.Holder.fields, [32 x i8] zeroinitializer, ptr @arith.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@arith.7 = hidden constant <{ [7 x i8] }> <{ [7 x i8] c"Holder\00" }>, align 1
@arith.8 = hidden constant <{ [5 x i8] }> <{ [5 x i8] c"kind\00" }>, align 1
@arith.9 = hidden constant <{ [3 x i8] }> <{ [3 x i8] c"on\00" }>, align 1
@arith.Holder.fields = hidden constant <{ ptr, [40 x i8], ptr, [40 x i8] }> <{ ptr @arith.8, [40 x i8] c"\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\17\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00", ptr @arith.9, [40 x i8] c"\02\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@arith.11 = hidden constant <{ [29 x i8] }> <{ [29 x i8] c"arith.anti:38: overflow in +\00" }>, align 1

declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, i64)
declare void @llvm.lifetime.start.p0(ptr)
declare void @llvm.lifetime.end.p0(ptr)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
!5 = !{i32 0, i32 3}
!6 = !{i8 0, i8 2}
!7 = !{!"anti"}
!8 = !{!"byte", !7, i64 0}
!9 = !{!"i16", !8, i64 0}
!10 = !{!"i32", !8, i64 0}
!11 = !{!"i64", !8, i64 0}
!12 = !{!"f32", !8, i64 0}
!13 = !{!"f64", !8, i64 0}
!14 = !{!"ptr", !8, i64 0}
!15 = !{!9, !9, i64 0}
!16 = !{!10, !10, i64 0}
!17 = !{!11, !11, i64 0}
!18 = !{!12, !12, i64 0}
!19 = !{!13, !13, i64 0}
!20 = !{!14, !14, i64 0}
!21 = !{!"arith.Point", !11, i64 0, !11, i64 8}
!22 = !{!"arith.Holder", !10, i64 0}
!23 = !{!21, !11, i64 8}
!24 = !{!22, !10, i64 0}
