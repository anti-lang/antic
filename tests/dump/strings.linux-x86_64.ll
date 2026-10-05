source_filename = "strings"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal { i64, i64 } @strings.tail(i64 %p0.0, i64 %p0.1, i64 noundef %p1) #0 {
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
  store i64 %p0.0, ptr %a0, align 8
  %v0 = getelementptr i8, ptr %a0, i64 8
  store i64 %p0.1, ptr %v0, align 8
  store ptr %a0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store ptr %s2, ptr %t2, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = load ptr, ptr %v1, align 8
  store ptr %v2, ptr %t3, align 8
  %v3 = load ptr, ptr %t0, align 8
  %v4 = getelementptr i8, ptr %v3, i64 8
  store ptr %v4, ptr %t4, align 8
  %v5 = load ptr, ptr %t4, align 8
  %v6 = load i64, ptr %v5, align 8
  store i64 %v6, ptr %t5, align 8
  %v7 = load ptr, ptr %t3, align 8
  %v8 = load i64, ptr %t1, align 8
  %v9 = getelementptr i8, ptr %v7, i64 %v8
  store ptr %v9, ptr %t6, align 8
  %v10 = load ptr, ptr %t6, align 8
  %v11 = load ptr, ptr %t2, align 8
  store ptr %v10, ptr %v11, align 8
  %v12 = load i64, ptr %t5, align 8
  %v13 = load i64, ptr %t1, align 8
  %v14 = sub i64 %v12, %v13
  store i64 %v14, ptr %t7, align 8
  %v15 = load ptr, ptr %t2, align 8
  %v16 = getelementptr i8, ptr %v15, i64 8
  store ptr %v16, ptr %t8, align 8
  %v17 = load i64, ptr %t7, align 8
  %v18 = load ptr, ptr %t8, align 8
  store i64 %v17, ptr %v18, align 8
  %v19 = load ptr, ptr %t2, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v19, i64 16, i1 false)
  %v20 = load i64, ptr %a1, align 8
  %v21 = getelementptr i8, ptr %a1, i64 8
  %v22 = load i64, ptr %v21, align 8
  %v23 = insertvalue { i64, i64 } poison, i64 %v20, 0
  %v24 = insertvalue { i64, i64 } %v23, i64 %v22, 1
  ret { i64, i64 } %v24
}

define internal noundef i64 @strings.main() #0 {
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
  %v6 = load i64, ptr %a0, align 8
  %v7 = getelementptr i8, ptr %a0, i64 8
  %v8 = load i64, ptr %v7, align 8
  %v9 = call { i64, i64 } (i64, i64, i64) @strings.tail(i64 %v6, i64 %v8, i64 1)
  %v10 = extractvalue { i64, i64 } %v9, 0
  store i64 %v10, ptr %a1, align 8
  %v11 = extractvalue { i64, i64 } %v9, 1
  %v12 = getelementptr i8, ptr %a1, i64 8
  store i64 %v11, ptr %v12, align 8
  store ptr %a1, ptr %t4, align 8
  %v13 = load ptr, ptr %t0, align 8
  %v14 = load ptr, ptr %t4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr %v13, ptr %v14, i64 16, i1 false)
  store ptr @strings.1, ptr %t5, align 8
  %v15 = load ptr, ptr %t5, align 8
  %v16 = call i32 (ptr) @puts(ptr %v15)
  store i32 %v16, ptr %t6, align 4
  %v17 = load ptr, ptr %t0, align 8
  %v18 = load ptr, ptr %v17, align 8
  store ptr %v18, ptr %t7, align 8
  %v19 = load ptr, ptr %t7, align 8
  %v20 = getelementptr i8, ptr %v19, i64 0
  store ptr %v20, ptr %t8, align 8
  %v21 = load ptr, ptr %t8, align 8
  %v22 = load i8, ptr %v21, align 1
  store i8 %v22, ptr %t9, align 1
  %v23 = load i8, ptr %t9, align 1
  %v24 = zext i8 %v23 to i64
  store i64 %v24, ptr %t10, align 8
  %v25 = load i64, ptr %t10, align 8
  ret i64 %v25
}

@anti.rt.main = alias i64 (), ptr @strings.main

@strings.0 = internal constant <{ [6 x i8] }> <{ [6 x i8] c"hello\00" }>, align 1
@strings.1 = internal constant <{ [3 x i8] }> <{ [3 x i8] c"hi\00" }>, align 1
@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @puts(ptr noundef) #1
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
