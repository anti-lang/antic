source_filename = "args"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @args.eight(i64 noundef %p0, i64 noundef %p1, i64 noundef %p2, i64 noundef %p3, i64 noundef %p4, i64 noundef %p5, i64 noundef %p6, i64 noundef %p7) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  store i64 %p2, ptr %t2, align 8
  store i64 %p3, ptr %t3, align 8
  store i64 %p4, ptr %t4, align 8
  store i64 %p5, ptr %t5, align 8
  store i64 %p6, ptr %t6, align 8
  store i64 %p7, ptr %t7, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = load i64, ptr %t7, align 8
  %v2 = add i64 %v0, %v1
  store i64 %v2, ptr %t8, align 8
  %v3 = load i64, ptr %t8, align 8
  ret i64 %v3
}

define internal noundef i64 @args.main() noinline #0 {
b0:
  %t0 = alloca ptr, align 8
  %t1 = alloca i8, align 1
  %t2 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i32, align 4
  %v0 = call ptr (i64) @malloc(i64 4)
  store ptr %v0, ptr %t0, align 8
  %v1 = load ptr, ptr %t0, align 8
  %v2 = icmp eq ptr %v1, null
  %v3 = zext i1 %v2 to i8
  store i8 %v3, ptr %t1, align 1
  %v4 = load i8, ptr %t1, align 1
  %v5 = trunc i8 %v4 to i1
  br i1 %v5, label %b1, label %b2

b1:
  ret i64 1

b2:
  %v6 = load ptr, ptr %t0, align 8
  %v7 = getelementptr i8, ptr %v6, i64 0
  store ptr %v7, ptr %t2, align 8
  %v8 = load ptr, ptr %t2, align 8
  store i8 37, ptr %v8, align 1
  %v9 = load ptr, ptr %t0, align 8
  %v10 = getelementptr i8, ptr %v9, i64 1
  store ptr %v10, ptr %t3, align 8
  %v11 = load ptr, ptr %t3, align 8
  store i8 100, ptr %v11, align 1
  %v12 = load ptr, ptr %t0, align 8
  %v13 = getelementptr i8, ptr %v12, i64 2
  store ptr %v13, ptr %t4, align 8
  %v14 = load ptr, ptr %t4, align 8
  store i8 10, ptr %v14, align 1
  %v15 = load ptr, ptr %t0, align 8
  %v16 = getelementptr i8, ptr %v15, i64 3
  store ptr %v16, ptr %t5, align 8
  %v17 = load ptr, ptr %t5, align 8
  store i8 0, ptr %v17, align 1
  %v18 = call i64 (i64, i64, i64, i64, i64, i64, i64, i64) @args.eight(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7, i64 8)
  store i64 %v18, ptr %t6, align 8
  %v19 = load ptr, ptr %t0, align 8
  %v20 = load i64, ptr %t6, align 8
  %v21 = call i32 (ptr, ...) @printf(ptr %v19, i64 %v20)
  store i32 %v21, ptr %t7, align 4
  ret i64 0
}

@anti.rt.main = alias i64 (), ptr @args.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8
@llvm.compiler.used = appending global [1 x ptr] [ptr @args.main], section "llvm.metadata"

declare noundef i32 @printf(ptr noundef, ...) #1
declare noundef ptr @malloc(i64 noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
