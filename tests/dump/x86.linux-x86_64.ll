source_filename = "x86"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define internal noundef i64 @x86.div(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = icmp eq i64 %v1, 0
  %v3 = icmp eq i64 %v1, -1
  %v4 = or i1 %v2, %v3
  %v5 = select i1 %v4, i64 1, i64 %v1
  %v6 = sdiv i64 %v0, %v5
  %v7 = sub i64 0, %v0
  %v8 = select i1 %v3, i64 %v7, i64 %v6
  %v9 = select i1 %v2, i64 0, i64 %v8
  store i64 %v9, ptr %t2, align 8
  %v10 = load i64, ptr %t0, align 8
  %v11 = load i64, ptr %t1, align 8
  %v12 = icmp eq i64 %v11, 0
  %v13 = icmp eq i64 %v11, -1
  %v14 = or i1 %v12, %v13
  %v15 = select i1 %v14, i64 1, i64 %v11
  %v16 = srem i64 %v10, %v15
  store i64 %v16, ptr %t3, align 8
  %v17 = load i64, ptr %t2, align 8
  %v18 = load i64, ptr %t3, align 8
  %v19 = add i64 %v17, %v18
  store i64 %v19, ptr %t4, align 8
  %v20 = load i64, ptr %t4, align 8
  ret i64 %v20
}

define internal noundef i32 @x86.ushift(i32 noundef %p0, i32 noundef %p1) #0 {
b0:
  %t0 = alloca i32, align 4
  %t1 = alloca i32, align 4
  %t2 = alloca i32, align 4
  %t3 = alloca i32, align 4
  %t4 = alloca i32, align 4
  store i32 %p0, ptr %t0, align 4
  store i32 %p1, ptr %t1, align 4
  %v0 = load i32, ptr %t0, align 4
  %v1 = load i32, ptr %t1, align 4
  %v2 = and i32 %v1, 31
  %v3 = lshr i32 %v0, %v2
  store i32 %v3, ptr %t2, align 4
  %v4 = load i32, ptr %t1, align 4
  %v5 = and i32 2, 31
  %v6 = shl i32 %v4, %v5
  store i32 %v6, ptr %t3, align 4
  %v7 = load i32, ptr %t2, align 4
  %v8 = load i32, ptr %t3, align 4
  %v9 = icmp eq i32 %v8, 0
  %v10 = select i1 %v9, i32 1, i32 %v8
  %v11 = udiv i32 %v7, %v10
  %v12 = select i1 %v9, i32 0, i32 %v11
  store i32 %v12, ptr %t4, align 4
  %v13 = load i32, ptr %t4, align 4
  ret i32 %v13
}

define internal noundef i64 @x86.bytes(i8 zeroext noundef %p0, i8 signext noundef %p1) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t3 = alloca i64, align 8
  %t4 = alloca i8, align 1
  %t5 = alloca i64, align 8
  %t6 = alloca i64, align 8
  store i8 %p0, ptr %t0, align 1
  store i8 %p1, ptr %t1, align 1
  %v0 = load i8, ptr %t0, align 1
  %v1 = mul i8 %v0, 3
  store i8 %v1, ptr %t2, align 1
  %v2 = load i8, ptr %t2, align 1
  %v3 = zext i8 %v2 to i64
  store i64 %v3, ptr %t3, align 8
  %v4 = load i8, ptr %t1, align 1
  %v5 = add i8 %v4, 1
  store i8 %v5, ptr %t4, align 1
  %v6 = load i8, ptr %t4, align 1
  %v7 = sext i8 %v6 to i64
  store i64 %v7, ptr %t5, align 8
  %v8 = load i64, ptr %t3, align 8
  %v9 = load i64, ptr %t5, align 8
  %v10 = add i64 %v8, %v9
  store i64 %v10, ptr %t6, align 8
  %v11 = load i64, ptr %t6, align 8
  ret i64 %v11
}

define internal noundef signext i16 @x86.narrow(i16 signext noundef %p0, i16 signext noundef %p1) #0 {
b0:
  %t0 = alloca i16, align 2
  %t1 = alloca i16, align 2
  %t2 = alloca i16, align 2
  %t3 = alloca i16, align 2
  %t4 = alloca i8, align 1
  %t5 = alloca i16, align 2
  %t6 = alloca i16, align 2
  %t7 = alloca i16, align 2
  %t8 = alloca i16, align 2
  store i16 %p0, ptr %t0, align 2
  store i16 %p1, ptr %t1, align 2
  %v0 = load i16, ptr %t0, align 2
  %v1 = load i16, ptr %t1, align 2
  %v2 = icmp eq i16 %v1, 0
  %v3 = icmp eq i16 %v1, -1
  %v4 = or i1 %v2, %v3
  %v5 = select i1 %v4, i16 1, i16 %v1
  %v6 = sdiv i16 %v0, %v5
  %v7 = sub i16 0, %v0
  %v8 = select i1 %v3, i16 %v7, i16 %v6
  %v9 = select i1 %v2, i16 0, i16 %v8
  store i16 %v9, ptr %t2, align 2
  %v10 = load i16, ptr %t0, align 2
  %v11 = load i16, ptr %t1, align 2
  %v12 = icmp eq i16 %v11, 0
  %v13 = icmp eq i16 %v11, -1
  %v14 = or i1 %v12, %v13
  %v15 = select i1 %v14, i16 1, i16 %v11
  %v16 = srem i16 %v10, %v15
  store i16 %v16, ptr %t3, align 2
  %v17 = load i16, ptr %t0, align 2
  %v18 = load i16, ptr %t1, align 2
  %v19 = icmp slt i16 %v17, %v18
  %v20 = zext i1 %v19 to i8
  store i8 %v20, ptr %t4, align 1
  %v21 = load i8, ptr %t4, align 1
  %v22 = trunc i8 %v21 to i1
  br i1 %v22, label %b1, label %b2

b1:
  %v23 = load i16, ptr %t2, align 2
  %v24 = load i16, ptr %t3, align 2
  %v25 = mul i16 %v23, %v24
  store i16 %v25, ptr %t5, align 2
  %v26 = load i16, ptr %t5, align 2
  ret i16 %v26

b2:
  %v27 = load i16, ptr %t2, align 2
  %v28 = and i16 1, 15
  %v29 = ashr i16 %v27, %v28
  store i16 %v29, ptr %t6, align 2
  %v30 = load i16, ptr %t3, align 2
  %v31 = and i16 3, 15
  %v32 = shl i16 %v30, %v31
  store i16 %v32, ptr %t7, align 2
  %v33 = load i16, ptr %t6, align 2
  %v34 = load i16, ptr %t7, align 2
  %v35 = xor i16 %v33, %v34
  store i16 %v35, ptr %t8, align 2
  %v36 = load i16, ptr %t8, align 2
  ret i16 %v36
}

define internal noundef i64 @x86.shifts(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  store i64 %p1, ptr %t1, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = and i64 %v1, 63
  %v3 = shl i64 %v0, %v2
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t0, align 8
  %v5 = load i64, ptr %t1, align 8
  %v6 = and i64 %v5, 63
  %v7 = ashr i64 %v4, %v6
  store i64 %v7, ptr %t3, align 8
  %v8 = load i64, ptr %t2, align 8
  %v9 = load i64, ptr %t3, align 8
  %v10 = add i64 %v8, %v9
  store i64 %v10, ptr %t4, align 8
  %v11 = load i64, ptr %t0, align 8
  %v12 = load i64, ptr %t1, align 8
  %v13 = and i64 %v12, 63
  %v14 = lshr i64 %v11, %v13
  store i64 %v14, ptr %t5, align 8
  %v15 = load i64, ptr %t4, align 8
  %v16 = load i64, ptr %t5, align 8
  %v17 = add i64 %v15, %v16
  store i64 %v17, ptr %t6, align 8
  %v18 = load i64, ptr %t6, align 8
  ret i64 %v18
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="x86-64-v3" "target-features"="+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
