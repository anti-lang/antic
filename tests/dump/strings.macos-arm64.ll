source_filename = "strings"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal [2 x i64] @strings.tail([2 x i64] %p0.0, i64 %p1) #0 {
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
  %a0 = alloca [16 x i8], align 8
  %s2 = alloca [16 x i8], align 8
  %a1 = alloca [16 x i8], align 8
  store [2 x i64] %p0.0, ptr %a0, align 8
  store ptr %a0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store ptr %s2, ptr %t2, align 8
  %v0 = load ptr, ptr %t0, align 8
  %v1 = load ptr, ptr %v0, align 8
  store ptr %v1, ptr %t3, align 8
  %v2 = load ptr, ptr %t0, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
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
  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v18, i64 16, i1 false)
  %v19 = load [2 x i64], ptr %a1, align 8
  ret [2 x i64] %v19
}

define internal i64 @strings.main() #0 {
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
  store ptr %s0, ptr %t0, align 8
  store ptr %s1, ptr %t1, align 8
  store ptr @strings.0, ptr %t2, align 8
  %v0 = load ptr, ptr %t2, align 8
  %v1 = load ptr, ptr %t1, align 8
  store ptr %v0, ptr %v1, align 8
  %v2 = load ptr, ptr %t1, align 8
  %v3 = getelementptr i8, ptr %v2, i64 8
  store ptr %v3, ptr %t3, align 8
  %v4 = load ptr, ptr %t3, align 8
  store i64 5, ptr %v4, align 8
  %v5 = load ptr, ptr %t1, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a0, ptr %v5, i64 16, i1 false)
  %v6 = load [2 x i64], ptr %a0, align 8
  %v7 = call [2 x i64] ([2 x i64], i64) @strings.tail([2 x i64] %v6, i64 1)
  store [2 x i64] %v7, ptr %a1, align 8
  store ptr %a1, ptr %t4, align 8
  %v8 = load ptr, ptr %t0, align 8
  %v9 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v8, ptr %v9, i64 16, i1 false)
  store ptr @strings.1, ptr %t5, align 8
  %v10 = load ptr, ptr %t5, align 8
  %v11 = call i32 (ptr) @puts(ptr %v10)
  store i32 %v11, ptr %t6, align 4
  %v12 = load ptr, ptr %t0, align 8
  %v13 = load ptr, ptr %v12, align 8
  store ptr %v13, ptr %t7, align 8
  %v14 = load ptr, ptr %t7, align 8
  %v15 = getelementptr i8, ptr %v14, i64 0
  store ptr %v15, ptr %t8, align 8
  %v16 = load ptr, ptr %t8, align 8
  %v17 = load i8, ptr %v16, align 1
  store i8 %v17, ptr %t9, align 1
  %v18 = load i8, ptr %t9, align 1
  %v19 = zext i8 %v18 to i64
  store i64 %v19, ptr %t10, align 8
  %v20 = load i64, ptr %t10, align 8
  ret i64 %v20
}

@anti.rt.main = alias i64 (), ptr @strings.main

@strings.0 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"hello\00" }>, align 1
@strings.1 = internal constant <{ [3 x i8] }> <{ [3 x i8] c"hi\00" }>, align 1
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare i32 @puts(ptr) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
