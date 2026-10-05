source_filename = "structs"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define internal void @_A7structs_call(ptr sret([16 x i8]) align 8 %sret, ptr %p0, ptr %p1, ptr %p2, i64 %p3.0) #0 !dbg !12 {
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
  %a0 = alloca [8 x i8], align 8
  %s4 = alloca [24 x i8], align 8
  %s5 = alloca [16 x i8], align 8
  %a1 = alloca [24 x i8], align 8
  %a2 = alloca [16 x i8], align 8
  %a3 = alloca [16 x i8], align 8
  %a4 = alloca [24 x i8], align 8
  %a5 = alloca [3 x i8], align 1
  %a6 = alloca [8 x i8], align 8
  store ptr %p0, ptr %t0, align 8
  store ptr %p1, ptr %t1, align 8
  store ptr %p2, ptr %t2, align 8
  store i64 %p3.0, ptr %a0, align 8
  store ptr %a0, ptr %t3, align 8
  store ptr %s4, ptr %t4, align 8, !dbg !13
  store ptr %s5, ptr %t5, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i64, ptr %v0, align 8
  store i64 %v1, ptr %t6, align 8
  %v2 = load i64, ptr %t6, align 8
  call void (ptr, i64) @make(ptr sret([24 x i8]) align 8 %a1, i64 %v2), !dbg !13
  store ptr %a1, ptr %t7, align 8
  %v3 = load ptr, ptr %t4, align 8
  %v4 = load ptr, ptr %t7, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v3, ptr %v4, i64 24, i1 false), !dbg !13
  %v5 = load ptr, ptr %t0, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a3, ptr %v5, i64 16, i1 false), !dbg !13
  %v6 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a4, ptr %v6, i64 24, i1 false), !dbg !13
  %v7 = load ptr, ptr %t2, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a5, ptr %v7, i64 3, i1 false), !dbg !13
  %v8 = load ptr, ptr %t3, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a6, ptr %v8, i64 8, i1 false), !dbg !13
  %v9 = load i64, ptr %a6, align 8
  call void (ptr, ptr, ptr, ptr, i64) @take(ptr sret([16 x i8]) align 8 %a2, ptr %a3, ptr %a4, ptr %a5, i64 %v9), !dbg !13
  store ptr %a2, ptr %t8, align 8
  %v10 = load ptr, ptr %t5, align 8
  %v11 = load ptr, ptr %t8, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v10, ptr %v11, i64 16, i1 false), !dbg !13
  %v12 = load ptr, ptr %t5, align 8
  %v13 = load i64, ptr %v12, align 8
  store i64 %v13, ptr %t9, align 8
  %v14 = load ptr, ptr %t1, align 8
  %v15 = getelementptr i8, ptr %v14, i64 16
  store ptr %v15, ptr %t10, align 8
  %v16 = load ptr, ptr %t10, align 8
  %v17 = load i64, ptr %v16, align 8
  store i64 %v17, ptr %t11, align 8
  %v18 = load i64, ptr %t9, align 8
  %v19 = load i64, ptr %t11, align 8
  %v20 = add i64 %v18, %v19
  store i64 %v20, ptr %t12, align 8
  %v21 = load i64, ptr %t12, align 8
  %v22 = load ptr, ptr %t5, align 8
  store i64 %v21, ptr %v22, align 8
  %v23 = load ptr, ptr %t5, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %sret, ptr %v23, i64 16, i1 false), !dbg !13
  ret void
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare void @take(ptr sret([16 x i8]) align 8, ptr, ptr, ptr, i64) #1
declare void @make(ptr sret([24 x i8]) align 8, i64 noundef) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind uwtable(sync) "frame-pointer"="none" "stack-probe-size"="4096" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !5, !6}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 2}
!1 = !{i32 7, !"uwtable", i32 2}
!2 = !{!"antic VERSION"}
!llvm.dbg.cu = !{!7}
!5 = !{i32 2, !"Debug Info Version", i32 3}
!6 = !{i32 2, !"CodeView", i32 1}
!7 = distinct !DICompileUnit(language: DW_LANG_C11, file: !8, producer: "antic VERSION", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!8 = !DIFile(filename: "structs.anti", directory: "")
!10 = !DISubroutineType(types: !11)
!11 = !{null}
!12 = distinct !DISubprogram(name: "structs.call", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!13 = !DILocation(line: 0, column: 0, scope: !12)
