source_filename = "com.example.scale"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define hidden noundef i64 @com.example.scale.scale(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca ptr, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %v0, i64 6)
  %v2 = mul nsw i64 %v0, 6
  %v3 = extractvalue { i64, i1 } %v1, 1
  store i64 %v2, ptr %t1, align 8
  br i1 %v3, label %b1, label %b2, !prof !3

b1:
  store ptr @com.example.scale.0, ptr %t2, align 8
  %v4 = load ptr, ptr %t2, align 8
  %v5 = load i64, ptr %t0, align 8
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v4, i64 39, i32 1, i64 %v5, i64 6)
  unreachable

b2:
  %v6 = load i64, ptr %t1, align 8
  ret i64 %v6
}

@com.example.scale.0 = hidden constant <{ [40 x i8] }> <{ [40 x i8] c"com/example/scale.anti:4: overflow in *\00" }>, align 1

declare void @anti_rt_check_failed(ptr noundef, i64 noundef, i32 noundef, i64 noundef, i64 noundef) noreturn cold #1
declare { i64, i1 } @llvm.smul.with.overflow.i64(i64, i64)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
!3 = !{!"branch_weights", i32 1, i32 2000}
