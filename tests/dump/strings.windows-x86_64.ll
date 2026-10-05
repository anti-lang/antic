source_filename = "strings"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define internal void @_A7strings_tail(ptr sret([16 x i8]) align 8 %sret, ptr %p0, i64 noundef %p1) #0 !dbg !12 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca ptr, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca ptr, align 8
  %s2 = alloca [16 x i8], align 8
  store ptr %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store ptr %s2, ptr %t2, align 8, !dbg !13
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %v0, align 8
  store ptr %v1, ptr %t3, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr inbounds i8, ptr %v2, i64 8
  store ptr %v3, ptr %t4, align 8
  %v4 = load ptr, ptr %t4, align 8
  %v5 = load i64, ptr %v4, align 8
  store i64 %v5, ptr %t5, align 8
  %v6 = load ptr, ptr %t3, align 8
  %v7 = load i64, ptr %t1, align 8
  %v8 = getelementptr i8, ptr %v6, i64 %v7
  store ptr %v8, ptr %t6, align 8
  %v9 = load ptr, ptr %t6, align 8
  %v10 = load ptr, ptr %t2, align 8
  store ptr %v9, ptr %v10, align 8
  %v11 = load i64, ptr %t5, align 8
  %v12 = load i64, ptr %t1, align 8
  %v13 = sub i64 %v11, %v12
  store i64 %v13, ptr %t7, align 8
  %v14 = load ptr, ptr %t2, align 8
  %v15 = getelementptr i8, ptr %v14, i64 8
  store ptr %v15, ptr %t8, align 8
  %v16 = load i64, ptr %t7, align 8
  %v17 = load ptr, ptr %t8, align 8
  store i64 %v16, ptr %v17, align 8
  %v18 = load ptr, ptr %t2, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %sret, ptr %v18, i64 16, i1 false), !dbg !13
  ret void
}

define internal noundef i64 @_A7strings_main() #0 !dbg !14 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i32, align 4
  %t7 = alloca ptr, align 8
  %t8 = alloca ptr, align 8
  %t9 = alloca i8, align 1
  %t10 = alloca i64, align 8
  %s0 = alloca [16 x i8], align 8
  %s1 = alloca [16 x i8], align 8
  %a0 = alloca [16 x i8], align 8
  %a1 = alloca [16 x i8], align 8
  store ptr %s0, ptr %t0, align 8, !dbg !15
  store ptr %s1, ptr %t1, align 8
  call void @llvm.lifetime.start.p0(ptr %s0), !dbg !15
  store ptr @_A7strings_0, ptr %t2, align 8
  %v0 = load ptr, ptr %t2, align 8
  %v1 = load ptr, ptr %t1, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t3, align 8
  %v4 = load ptr, ptr %t3, align 8
  store i64 5, ptr %v4, align 8
  %v5 = load ptr, ptr %t1, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v5, i64 16, i1 false), !dbg !15
  call void (ptr, ptr, i64) @_A7strings_tail(ptr sret([16 x i8]) align 8 %a0, ptr %a1, i64 1), !dbg !15
  store ptr %a0, ptr %t4, align 8
  %v6 = load ptr, ptr %t0, align 8
  %v7 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v6, ptr %v7, i64 16, i1 false), !dbg !15
  store ptr @_A7strings_1, ptr %t5, align 8
  %v8 = load ptr, ptr %t5, align 8
  %v9 = call i32 (ptr) @puts(ptr %v8), !dbg !15
  store i32 %v9, ptr %t6, align 4
  %v10 = load ptr, ptr %t0, align 8
  %v11 = load ptr, ptr %v10, align 8
  store ptr %v11, ptr %t7, align 8
  %v12 = load ptr, ptr %t7, align 8
  %v13 = getelementptr i8, ptr %v12, i64 0
  store ptr %v13, ptr %t8, align 8
  %v14 = load ptr, ptr %t8, align 8
  %v15 = load i8, ptr %v14, align 1
  store i8 %v15, ptr %t9, align 1
  %v16 = load i8, ptr %t9, align 1
  %v17 = zext i8 %v16 to i64
  store i64 %v17, ptr %t10, align 8
  %v18 = load i64, ptr %t10, align 8
  ret i64 %v18
}

@_A4anti2rt_main = alias i64 (), ptr @_A7strings_main

@_A7strings_0 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"hello\00" }>, align 1
@_A7strings_1 = internal constant <{ [3 x i8] }> <{ [3 x i8] c"hi\00" }>, align 1
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @puts(ptr noundef) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.lifetime.start.p0(ptr)

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
!8 = !DIFile(filename: "strings.anti", directory: "")
!10 = !DISubroutineType(types: !11)
!11 = !{null}
!12 = distinct !DISubprogram(name: "strings.tail", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!13 = !DILocation(line: 0, column: 0, scope: !12)
!14 = distinct !DISubprogram(name: "strings.main", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!15 = !DILocation(line: 0, column: 0, scope: !14)
