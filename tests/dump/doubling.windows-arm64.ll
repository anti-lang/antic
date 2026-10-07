source_filename = "com.example.doubling"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-p:64:64-i32:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-pc-windows-msvc"

define dso_local noundef i64 @twice(i64 noundef %p0) #0 !dbg !13 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8, !dbg !14
  %v1 = and i64 1, 63
  %v2 = shl i64 %v0, %v1
  store i64 %v2, ptr %t1, align 8
  %v3 = load i64, ptr %t1, align 8
  ret i64 %v3
}

@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @anti_rt_init, ptr null }]

declare void @anti_rt_init() #1

attributes #0 = { nounwind uwtable(sync) "frame-pointer"="none" "stack-probe-size"="4096" "target-cpu"="generic" "target-features"="+crc,+dotprod,+fp-armv8,+fullfp16,+lse,+neon,+ras,+rdm,+v8.1a,+v8.2a,+v8a,-fmv" }
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
!9 = !DIFile(filename: "com/example/doubling.anti", directory: "")
!11 = !DISubroutineType(types: !12)
!12 = !{null}
!13 = distinct !DISubprogram(name: "twice", scope: !9, file: !9, line: 0, type: !11, scopeLine: 0, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !8)
!14 = !DILocation(line: 0, column: 0, scope: !13)
