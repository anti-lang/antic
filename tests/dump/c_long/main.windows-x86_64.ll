source_filename = "main"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define internal noundef i32 @_A4main_f() #0 !dbg !13 {
b0:
  ret i32 292552704, !dbg !14
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

attributes #0 = { nounwind uwtable(sync) "frame-pointer"="none" "stack-probe-size"="4096" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !6, !7}
!llvm.ident = !{!3}
!0 = !{i32 1, !"wchar_size", i32 2}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
!3 = !{!"antic VERSION"}
!llvm.dbg.cu = !{!8}
!6 = !{i32 2, !"Debug Info Version", i32 3}
!7 = !{i32 2, !"CodeView", i32 1}
!8 = distinct !DICompileUnit(language: DW_LANG_C11, file: !9, producer: "antic VERSION", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!9 = !DIFile(filename: "main.anti", directory: "")
!11 = !DISubroutineType(types: !12)
!12 = !{null}
!13 = distinct !DISubprogram(name: "main.f", scope: !9, file: !9, line: 0, type: !11, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8)
!14 = !DILocation(line: 0, column: 0, scope: !13)
