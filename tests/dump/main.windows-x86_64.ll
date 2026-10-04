source_filename = "main"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define internal i64 @_A4main_scale(i64 %p0) #0 !dbg !12 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8, !dbg !13
  %v1 = mul i64 %v0, 6
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t1, align 8
  ret i64 %v2
}

define internal i64 @_A4main_main() #0 !dbg !14 {
b0:
  %t0 = alloca i64, align 8
  %v0 = call i64 (i64) @_A4main_scale(i64 7), !dbg !15
  store i64 %v0, ptr %t0, align 8
  %v1 = load i64, ptr %t0, align 8
  ret i64 %v1
}

@_A4anti2rt_main = alias i64 (), ptr @_A4main_main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

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
!8 = !DIFile(filename: "main.anti", directory: "")
!10 = !DISubroutineType(types: !11)
!11 = !{null}
!12 = distinct !DISubprogram(name: "main.scale", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!13 = !DILocation(line: 0, column: 0, scope: !12)
!14 = distinct !DISubprogram(name: "main.main", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!15 = !DILocation(line: 0, column: 0, scope: !14)
