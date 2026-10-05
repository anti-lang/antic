source_filename = "com.example.scale"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define i64 @_A3com7example5scale_scale(i64 %p0) #0 !dbg !12 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8, !dbg !13
  %v1 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %v0, i64 6), !dbg !13
  %v2 = extractvalue { i64, i1 } %v1, 0
  %v3 = extractvalue { i64, i1 } %v1, 1
  store i64 %v2, ptr %t1, align 8
  br i1 %v3, label %b1, label %b2, !prof !3

b1:
  store ptr @_A3com7example5scale_0, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  %v5 = load i64, ptr %t0, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v4, i64 39, i32 1, i64 %v5, i64 6), !dbg !13
  unreachable

b2:
  %v6 = load i64, ptr %t1, align 8
  ret i64 %v6
}

@_A3com7example5scale_0 = constant <{ [40 x i8] }> <{ [40 x i8] c"com/example/scale.anti:4: overflow in *\00" }>, align 1

declare void @anti_rt_check_failed(ptr, i64, i32, i64, i64) noreturn cold #1
declare { i64, i1 } @llvm.smul.with.overflow.i64(i64, i64)

attributes #0 = { nounwind uwtable(sync) "frame-pointer"="none" "stack-probe-size"="4096" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !5, !6}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 2}
!1 = !{i32 7, !"uwtable", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
!llvm.dbg.cu = !{!7}
!5 = !{i32 2, !"Debug Info Version", i32 3}
!6 = !{i32 2, !"CodeView", i32 1}
!7 = distinct !DICompileUnit(language: DW_LANG_C11, file: !8, producer: "antic VERSION", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!8 = !DIFile(filename: "com/example/scale.anti", directory: "")
!10 = !DISubroutineType(types: !11)
!11 = !{null}
!12 = distinct !DISubprogram(name: "com.example.scale.scale", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagDefinition, unit: !7)
!13 = !DILocation(line: 0, column: 0, scope: !12)
