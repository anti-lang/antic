source_filename = "cell"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i64 @cell.cell(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca i64, align 8
  %s2 = alloca [8 x i8], align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store ptr %s2, ptr %t2, align 8
  call void @llvm.lifetime.start.p0(ptr %s2)
  %v0 = load i64, ptr %t0, align 8
  %v1 = load ptr, ptr %t2, align 8
  store i64 %v0, ptr %v1, align 8
  %v2 = load i64, ptr %t0, align 8
  %v3 = load i64, ptr %t1, align 8
  %v4 = add i64 %v2, %v3
  store i64 %v4, ptr %t3, align 8
  %v5 = load i64, ptr %t3, align 8
  %v6 = load ptr, ptr %t2, align 8
  store i64 %v5, ptr %v6, align 8, !tbaa !15
  %v7 = load i64, ptr %t3, align 8
  ret i64 %v7
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare void @llvm.lifetime.start.p0(ptr)

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
