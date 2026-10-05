source_filename = "structs"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-p:64:64-i32:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-pc-windows-msvc"

define internal [2 x i64] @_A7structs_call([2 x i64] %p0.0, ptr %p1, [1 x i64] %p2.0, [2 x float] %p3.0) #0 !dbg !30 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca ptr, align 8
  %t8 = alloca ptr, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca ptr, align 8
  %t11 = alloca i64, align 8
  %t12 = alloca i64, align 8
  %a0 = alloca [16 x i8], align 8
  %a1 = alloca [8 x i8], align 8
  %a2 = alloca [8 x i8], align 8
  %s4 = alloca [24 x i8], align 8
  %s5 = alloca [16 x i8], align 8
  %a3 = alloca [24 x i8], align 8
  %a4 = alloca [16 x i8], align 8
  %a5 = alloca [24 x i8], align 8
  %a6 = alloca [8 x i8], align 8
  %a7 = alloca [8 x i8], align 8
  %a8 = alloca [16 x i8], align 8
  %a9 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store [1 x i64] %p2.0, ptr %a1, align 8
  store ptr %a1, ptr %t2, align 8
  store [2 x float] %p3.0, ptr %a2, align 8
  store ptr %a2, ptr %t3, align 8
  store ptr %s4, ptr %t4, align 8, !dbg !31
  store ptr %s5, ptr %t5, align 8
  call void @llvm.lifetime.start.p0(ptr %s4), !dbg !31
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i64, ptr %v0, align 8, !tbaa !21
  store i64 %v1, ptr %t6, align 8
  %v2 = load i64, ptr %t6, align 8
  call void (ptr, i64) @make(ptr sret([24 x i8]) align 8 %a3, i64 %v2), !dbg !31
  store ptr %a3, ptr %t7, align 8
  %v3 = load ptr, ptr %t4, align 8
  %v4 = load ptr, ptr %t7, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v3, ptr %v4, i64 24, i1 false), !dbg !31
  call void @llvm.lifetime.start.p0(ptr %s5), !dbg !31
  %v5 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a4, ptr %v5, i64 16, i1 false), !dbg !31
  %v6 = load [2 x i64], ptr %a4, align 8
  %v7 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a5, ptr %v7, i64 24, i1 false), !dbg !31
  %v8 = load ptr, ptr %t2, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a6, ptr %v8, i64 3, i1 false), !dbg !31
  %v9 = load [1 x i64], ptr %a6, align 8
  %v10 = load ptr, ptr %t3, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a7, ptr %v10, i64 8, i1 false), !dbg !31
  %v11 = load [2 x float], ptr %a7, align 8
  %v12 = call [2 x i64] ([2 x i64], ptr, [1 x i64], [2 x float]) @take([2 x i64] %v6, ptr %a5, [1 x i64] %v9, [2 x float] %v11), !dbg !31
  store [2 x i64] %v12, ptr %a8, align 8
  store ptr %a8, ptr %t8, align 8
  %v13 = load ptr, ptr %t5, align 8
  %v14 = load ptr, ptr %t8, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v13, ptr %v14, i64 16, i1 false), !dbg !31
  %v15 = load ptr, ptr %t5, align 8
  %v16 = load i64, ptr %v15, align 8, !tbaa !21
  store i64 %v16, ptr %t9, align 8
  %v17 = load ptr, ptr %t1, align 8
  %v18 = getelementptr inbounds i8, ptr %v17, i64 16
  store ptr %v18, ptr %t10, align 8
  %v19 = load ptr, ptr %t10, align 8
  %v20 = load i64, ptr %v19, align 8, !tbaa !22
  store i64 %v20, ptr %t11, align 8
  %v21 = load i64, ptr %t9, align 8
  %v22 = load i64, ptr %t11, align 8
  %v23 = add i64 %v21, %v22
  store i64 %v23, ptr %t12, align 8
  %v24 = load i64, ptr %t12, align 8
  %v25 = load ptr, ptr %t5, align 8
  store i64 %v24, ptr %v25, align 8, !tbaa !21
  %v26 = load ptr, ptr %t5, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a9, ptr %v26, i64 16, i1 false), !dbg !31
  %v27 = load [2 x i64], ptr %a9, align 8
  ret [2 x i64] %v27
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare [2 x i64] @take([2 x i64], ptr, [1 x i64], [2 x float]) #1
declare void @make(ptr sret([24 x i8]) align 8, i64 noundef) #1
declare void @llvm.lifetime.start.p0(ptr)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind uwtable(sync) "frame-pointer"="none" "stack-probe-size"="4096" "target-cpu"="generic" "target-features"="+crc,+dotprod,+fp-armv8,+fullfp16,+lse,+neon,+ras,+rdm,+v8.1a,+v8.2a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !23, !24}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 2}
!1 = !{i32 7, !"uwtable", i32 2}
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
!19 = !{!"structs.Pair", !9, i64 0, !11, i64 8}
!20 = !{!"structs.Big", !9, i64 0, !9, i64 8, !9, i64 16}
!21 = !{!19, !9, i64 0}
!22 = !{!20, !9, i64 16}
!llvm.dbg.cu = !{!25}
!23 = !{i32 2, !"Debug Info Version", i32 3}
!24 = !{i32 2, !"CodeView", i32 1}
!25 = distinct !DICompileUnit(language: DW_LANG_C11, file: !26, producer: "antic VERSION", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!26 = !DIFile(filename: "structs.anti", directory: "")
!28 = !DISubroutineType(types: !29)
!29 = !{null}
!30 = distinct !DISubprogram(name: "structs.call", scope: !26, file: !26, line: 0, type: !28, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !25)
!31 = !DILocation(line: 0, column: 0, scope: !30)
