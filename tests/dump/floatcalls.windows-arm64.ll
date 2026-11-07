source_filename = "floatcalls"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-p:64:64-i32:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-pc-windows-msvc"

define internal noundef double @_A10floatcalls_calls(ptr noundef nonnull dereferenceable(1) %p0, double noundef %p1) #0 !dbg !13 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca double, align 8
  %t2 = alloca i32, align 4
  %t3 = alloca double, align 8
  %t4 = alloca float, align 4
  %t5 = alloca double, align 8
  %t6 = alloca double, align 8
  store ptr %p0, ptr %t0, align 8
  store double %p1, ptr %t1, align 8
  %v0 = load ptr, ptr %t0, align 8, !dbg !14
  %v1 = load double, ptr %t1, align 8
  %v2 = fcmp uno double %v1, 0.0
  %v3 = select i1 %v2, double 0x7FF8000000000000, double %v1
  %v4 = call i32 (ptr, ...) @printf(ptr %v0, double %v3, i64 3), !dbg !14
  store i32 %v4, ptr %t2, align 4
  %v5 = load double, ptr %t1, align 8
  %v6 = fcmp uno double %v5, 0.0
  %v7 = select i1 %v6, double 0x7FF8000000000000, double %v5
  %v8 = call double (i64, double, i64, float) @mix(i64 1, double %v7, i64 2, float 0x3FE0000000000000), !dbg !14
  store double %v8, ptr %t3, align 8
  %v9 = load double, ptr %t1, align 8
  %v10 = fcmp uno double %v9, 0.0
  %v11 = select i1 %v10, double 0x7FF8000000000000, double %v9
  %v12 = load double, ptr %t1, align 8
  %v13 = fcmp uno double %v12, 0.0
  %v14 = select i1 %v13, double 0x7FF8000000000000, double %v12
  %v15 = load double, ptr %t1, align 8
  %v16 = fcmp uno double %v15, 0.0
  %v17 = select i1 %v16, double 0x7FF8000000000000, double %v15
  %v18 = load double, ptr %t1, align 8
  %v19 = fcmp uno double %v18, 0.0
  %v20 = select i1 %v19, double 0x7FF8000000000000, double %v18
  %v21 = load double, ptr %t1, align 8
  %v22 = fcmp uno double %v21, 0.0
  %v23 = select i1 %v22, double 0x7FF8000000000000, double %v21
  %v24 = load double, ptr %t1, align 8
  %v25 = fcmp uno double %v24, 0.0
  %v26 = select i1 %v25, double 0x7FF8000000000000, double %v24
  %v27 = load double, ptr %t1, align 8
  %v28 = fcmp uno double %v27, 0.0
  %v29 = select i1 %v28, double 0x7FF8000000000000, double %v27
  %v30 = load double, ptr %t1, align 8
  %v31 = fcmp uno double %v30, 0.0
  %v32 = select i1 %v31, double 0x7FF8000000000000, double %v30
  %v33 = call float (double, double, double, double, double, double, double, double, float, float) @nine(double %v11, double %v14, double %v17, double %v20, double %v23, double %v26, double %v29, double %v32, float 0x3FF0000000000000, float 0x4000000000000000), !dbg !14
  store float %v33, ptr %t4, align 4
  %v34 = load float, ptr %t4, align 4
  %v35 = fpext float %v34 to double
  store double %v35, ptr %t5, align 8
  %v36 = load double, ptr %t3, align 8
  %v37 = load double, ptr %t5, align 8
  %v38 = fadd double %v36, %v37
  store double %v38, ptr %t6, align 8
  %v39 = load double, ptr %t6, align 8
  ret double %v39
}

define internal noundef float @_A10floatcalls_last(double noundef %p0, double noundef %p1, double noundef %p2, double noundef %p3, double noundef %p4, double noundef %p5, double noundef %p6, double noundef %p7, float noundef %p8, float noundef %p9) #0 !dbg !15 {
b0:
  %t0 = alloca double, align 8
  %t1 = alloca double, align 8
  %t2 = alloca double, align 8
  %t3 = alloca double, align 8
  %t4 = alloca double, align 8
  %t5 = alloca double, align 8
  %t6 = alloca double, align 8
  %t7 = alloca double, align 8
  %t8 = alloca float, align 4
  %t9 = alloca float, align 4
  store double %p0, ptr %t0, align 8
  store double %p1, ptr %t1, align 8
  store double %p2, ptr %t2, align 8
  store double %p3, ptr %t3, align 8
  store double %p4, ptr %t4, align 8
  store double %p5, ptr %t5, align 8
  store double %p6, ptr %t6, align 8
  store double %p7, ptr %t7, align 8
  store float %p8, ptr %t8, align 4
  store float %p9, ptr %t9, align 4
  %v0 = load float, ptr %t9, align 4, !dbg !16
  ret float %v0
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @printf(ptr noundef, ...) #1
declare noundef double @mix(i64 noundef, double noundef, i64 noundef, float noundef) #1
declare noundef float @nine(double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, double noundef, float noundef, float noundef) #1

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
!9 = !DIFile(filename: "floatcalls.anti", directory: "")
!11 = !DISubroutineType(types: !12)
!12 = !{null}
!13 = distinct !DISubprogram(name: "floatcalls.calls", scope: !9, file: !9, line: 0, type: !11, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8)
!14 = !DILocation(line: 0, column: 0, scope: !13)
!15 = distinct !DISubprogram(name: "floatcalls.last", scope: !9, file: !9, line: 0, type: !11, scopeLine: 0, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8)
!16 = !DILocation(line: 0, column: 0, scope: !15)
