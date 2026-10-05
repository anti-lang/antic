source_filename = "bitfields"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

define internal noundef i64 @_A9bitfields_main() #0 !dbg !12 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i32, align 4
  %t2 = alloca i32, align 4
  %t3 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t5 = alloca i32, align 4
  %t6 = alloca i64, align 8
  %t7 = alloca i32, align 4
  %t8 = alloca i64, align 8
  %t9 = alloca i64, align 8
  %t10 = alloca i64, align 8
  %t11 = alloca i8, align 1
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca i64, align 8
  %t15 = alloca i16, align 2
  %t16 = alloca i64, align 8
  %t17 = alloca i64, align 8
  %s0 = alloca [8 x i8], align 4
  store ptr %s0, ptr %t0, align 8, !dbg !13
  call void @llvm.lifetime.start.p0(ptr %s0), !dbg !13
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load i8, ptr %v0, align 1
  %v2 = trunc i32 1 to i8
  %v3 = and i8 %v2, 1
  %v4 = and i8 %v1, -2
  %v5 = or i8 %v4, %v3
  store i8 %v5, ptr %v0, align 1
  %v6 = load ptr, ptr %t0, align 8
  %v7 = load i8, ptr %v6, align 1
  %v8 = trunc i32 9 to i8
  %v9 = and i8 %v8, 15
  %v10 = shl i8 %v9, 1
  %v11 = and i8 %v7, -31
  %v12 = or i8 %v11, %v10
  store i8 %v12, ptr %v6, align 1
  %v13 = load ptr, ptr %t0, align 8
  %v14 = getelementptr i8, ptr %v13, i64 4
  %v15 = load i8, ptr %v14, align 1
  %v16 = and i8 -3, 7
  %v17 = and i8 %v15, -8
  %v18 = or i8 %v17, %v16
  store i8 %v18, ptr %v14, align 1
  %v19 = load ptr, ptr %t0, align 8
  %v20 = getelementptr i8, ptr %v19, i64 6
  %v21 = load i16, ptr %v20, align 2
  %v22 = and i16 300, 511
  %v23 = and i16 %v21, -512
  %v24 = or i16 %v23, %v22
  store i16 %v24, ptr %v20, align 2
  %v25 = load ptr, ptr %t0, align 8
  %v26 = load i8, ptr %v25, align 1
  %v27 = lshr i8 %v26, 1
  %v28 = and i8 %v27, 15
  %v29 = zext i8 %v28 to i32
  store i32 %v29, ptr %t1, align 4
  %v30 = load i32, ptr %t1, align 4
  %v31 = add i32 %v30, 3
  store i32 %v31, ptr %t2, align 4
  %v32 = load i32, ptr %t2, align 4
  %v33 = load ptr, ptr %t0, align 8
  %v34 = load i8, ptr %v33, align 1
  %v35 = trunc i32 %v32 to i8
  %v36 = and i8 %v35, 15
  %v37 = shl i8 %v36, 1
  %v38 = and i8 %v34, -31
  %v39 = or i8 %v38, %v37
  store i8 %v39, ptr %v33, align 1
  %v40 = load ptr, ptr %t0, align 8
  %v41 = getelementptr i8, ptr %v40, i64 4
  %v42 = load i8, ptr %v41, align 1
  %v43 = shl i8 %v42, 5
  %v44 = ashr i8 %v43, 5
  store i8 %v44, ptr %t3, align 1
  %v45 = load i8, ptr %t3, align 1
  %v46 = sub i8 %v45, 1
  store i8 %v46, ptr %t4, align 1
  %v47 = load i8, ptr %t4, align 1
  %v48 = load ptr, ptr %t0, align 8
  %v49 = getelementptr i8, ptr %v48, i64 4
  %v50 = load i8, ptr %v49, align 1
  %v51 = and i8 %v47, 7
  %v52 = and i8 %v50, -8
  %v53 = or i8 %v52, %v51
  store i8 %v53, ptr %v49, align 1
  %v54 = load ptr, ptr %t0, align 8
  %v55 = load i8, ptr %v54, align 1
  %v56 = and i8 %v55, 1
  %v57 = zext i8 %v56 to i32
  store i32 %v57, ptr %t5, align 4
  %v58 = load i32, ptr %t5, align 4
  %v59 = zext i32 %v58 to i64
  store i64 %v59, ptr %t6, align 8
  %v60 = load ptr, ptr %t0, align 8
  %v61 = load i8, ptr %v60, align 1
  %v62 = lshr i8 %v61, 1
  %v63 = and i8 %v62, 15
  %v64 = zext i8 %v63 to i32
  store i32 %v64, ptr %t7, align 4
  %v65 = load i32, ptr %t7, align 4
  %v66 = zext i32 %v65 to i64
  store i64 %v66, ptr %t8, align 8
  %v67 = load i64, ptr %t8, align 8
  %v68 = mul i64 %v67, 10
  store i64 %v68, ptr %t9, align 8
  %v69 = load i64, ptr %t6, align 8
  %v70 = load i64, ptr %t9, align 8
  %v71 = add i64 %v69, %v70
  store i64 %v71, ptr %t10, align 8
  %v72 = load ptr, ptr %t0, align 8
  %v73 = getelementptr i8, ptr %v72, i64 4
  %v74 = load i8, ptr %v73, align 1
  %v75 = shl i8 %v74, 5
  %v76 = ashr i8 %v75, 5
  store i8 %v76, ptr %t11, align 1
  %v77 = load i8, ptr %t11, align 1
  %v78 = sext i8 %v77 to i64
  store i64 %v78, ptr %t12, align 8
  %v79 = load i64, ptr %t12, align 8
  %v80 = mul i64 %v79, 100
  store i64 %v80, ptr %t13, align 8
  %v81 = load i64, ptr %t10, align 8
  %v82 = load i64, ptr %t13, align 8
  %v83 = add i64 %v81, %v82
  store i64 %v83, ptr %t14, align 8
  %v84 = load ptr, ptr %t0, align 8
  %v85 = getelementptr i8, ptr %v84, i64 6
  %v86 = load i16, ptr %v85, align 2
  %v87 = and i16 %v86, 511
  store i16 %v87, ptr %t15, align 2
  %v88 = load i16, ptr %t15, align 2
  %v89 = zext i16 %v88 to i64
  store i64 %v89, ptr %t16, align 8
  %v90 = load i64, ptr %t14, align 8
  %v91 = load i64, ptr %t16, align 8
  %v92 = add i64 %v90, %v91
  store i64 %v92, ptr %t17, align 8
  %v93 = load i64, ptr %t17, align 8
  ret i64 %v93
}

@_A4anti2rt_main = alias i64 (), ptr @_A9bitfields_main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

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
!8 = !DIFile(filename: "bitfields.anti", directory: "")
!10 = !DISubroutineType(types: !11)
!11 = !{null}
!12 = distinct !DISubprogram(name: "bitfields.main", scope: !8, file: !8, line: 0, type: !10, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !7)
!13 = !DILocation(line: 0, column: 0, scope: !12)
