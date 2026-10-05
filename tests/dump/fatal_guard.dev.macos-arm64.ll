source_filename = "fatal_guard"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define hidden noundef i64 @fatal_guard.pick(ptr noundef %p0) #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca i8, align 1
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca i64, align 8
  %s1 = alloca [8 x i8], align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %s1, ptr %t1, align 8
  call void @llvm.lifetime.start.p0(ptr %s1)
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %t1, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = icmp eq ptr %v2, null
  %v4 = zext i1 %v3 to i8
  store i8 %v4, ptr %t2, align 1
  %v5 = load i8, ptr %t2, align 1
  %v6 = trunc i8 %v5 to i1
  br i1 %v6, label %b1, label %b2, !prof !3

b1:
  %v7 = call ptr () @anti.lang.NoneDereference.new()
  store ptr %v7, ptr %t3, align 8
  %v8 = load ptr, ptr %t3, align 8
  call void (ptr) @anti.lang.Error.fatal(ptr %v8)
  unreachable

b2:
  %v9 = load ptr, ptr %t1, align 8
  %v10 = load ptr, ptr %v9, align 8, !tbaa !18
  store ptr %v10, ptr %t4, align 8
  %v11 = load ptr, ptr %t4, align 8
  %v12 = load i64, ptr %v11, align 8, !tbaa !20
  store i64 %v12, ptr %t5, align 8
  %v13 = load i64, ptr %t5, align 8
  ret i64 %v13
}

define hidden void @fatal_guard.stop(i64 noundef %p0) noreturn cold #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i32, align 4
  %t2 = alloca i64, align 8
  %t3 = alloca i8, align 1
  %t4 = alloca ptr, align 8
  %t5 = alloca i32, align 4
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = trunc i64 %v0 to i32
  store i32 %v1, ptr %t1, align 4
  %v2 = load i32, ptr %t1, align 4
  %v3 = sext i32 %v2 to i64
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  %v5 = load i64, ptr %t0, align 8
  %v6 = icmp eq i64 %v4, %v5
  %v7 = zext i1 %v6 to i8
  store i8 %v7, ptr %t3, align 1
  %v8 = load i8, ptr %t3, align 1
  %v9 = trunc i8 %v8 to i1
  br i1 %v9, label %b2, label %b1, !prof !4

b1:
  store ptr @fatal_guard.5, ptr %t4, align 8
  %v10 = load ptr, ptr %t4, align 8
  %v11 = load i64, ptr %t0, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v10, i64 47, i32 2, i64 %v11, i64 0)
  unreachable

b2:
  %v12 = load i64, ptr %t0, align 8
  %v13 = trunc i64 %v12 to i32
  store i32 %v13, ptr %t5, align 4
  %v14 = load i32, ptr %t5, align 4
  call void (i32) @anti_rt_exit(i32 %v14)
  unreachable
}

@anti_lang_Object_descriptor = external global [0 x i8], align 1
@fatal_guard.Cell.descriptor = hidden constant <{ ptr, [48 x i8], ptr, [32 x i8], ptr, [32 x i8] }> <{ ptr @fatal_guard.1, [48 x i8] c"\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00", ptr @fatal_guard.Cell.fields, [32 x i8] zeroinitializer, ptr @fatal_guard.package.version, [32 x i8] c"\05\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@fatal_guard.1 = hidden constant <{ [5 x i8] }> <{ [5 x i8] c"Cell\00" }>, align 1
@fatal_guard.2 = hidden constant <{ [2 x i8] }> <{ [2 x i8] c"v\00" }>, align 1
@fatal_guard.Cell.fields = hidden constant <{ ptr, [40 x i8] }> <{ ptr @fatal_guard.2, [40 x i8] c"\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" }>, align 8
@fatal_guard.package.version = hidden constant <{ [6 x i8] }> <{ [6 x i8] c"0.0.0\00" }>, align 1
@fatal_guard.5 = hidden constant <{ [48 x i8] }> <{ [48 x i8] c"fatal_guard.anti:23: value out of range for i32\00" }>, align 1

declare void @anti_rt_exit(i32 noundef) noreturn cold #1
declare void @anti.lang.Error.fatal(ptr noundef nonnull dereferenceable(120)) noreturn cold #1
declare noundef ptr @anti.lang.NoneDereference.new() #1
declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare void @llvm.lifetime.start.p0(ptr)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
!4 = !{!"branch_weights", i32 2000, i32 1}
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
!19 = !{!"fatal_guard.Cell", !9, i64 0}
!20 = !{!19, !9, i64 0}
