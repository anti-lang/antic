source_filename = "arm64"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i64 @arm64.imms(i64 noundef %p0) #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i8, align 1
  %t6 = alloca i64, align 8
  store i64 %p0, ptr %t0, align 8
  %v0 = load i64, ptr %t0, align 8
  %v1 = and i64 %v0, 65280
  store i64 %v1, ptr %t1, align 8
  %v2 = load i64, ptr %t1, align 8
  %v3 = or i64 %v2, 6148914691236517205
  store i64 %v3, ptr %t2, align 8
  %v4 = load i64, ptr %t2, align 8
  %v5 = xor i64 %v4, 4660
  store i64 %v5, ptr %t3, align 8
  %v6 = load i64, ptr %t3, align 8
  %v7 = add i64 %v6, 12288
  store i64 %v7, ptr %t4, align 8
  %v8 = load i64, ptr %t4, align 8
  %v9 = icmp slt i64 %v8, 28672
  %v10 = zext i1 %v9 to i8
  store i8 %v10, ptr %t5, align 1
  %v11 = load i8, ptr %t5, align 1
  %v12 = trunc i8 %v11 to i1
  br i1 %v12, label %b1, label %b2

b1:
  %v13 = load i64, ptr %t4, align 8
  %v14 = sub i64 %v13, 8192
  store i64 %v14, ptr %t6, align 8
  %v15 = load i64, ptr %t6, align 8
  ret i64 %v15

b2:
  %v16 = load i64, ptr %t4, align 8
  ret i64 %v16
}

define internal noundef zeroext i8 @arm64.bump(i8 zeroext noundef %p0) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i8, align 1
  store i8 %p0, ptr %t0, align 1
  %v0 = load i8, ptr %t0, align 1
  %v1 = add i8 %v0, 1
  store i8 %v1, ptr %t1, align 1
  %v2 = load i8, ptr %t1, align 1
  ret i8 %v2
}

define internal noundef i32 @arm64.pass(i8 zeroext noundef %p0, i8 signext noundef %p1) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t3 = alloca i32, align 4
  store i8 %p0, ptr %t0, align 1
  store i8 %p1, ptr %t1, align 1
  %v0 = load i8, ptr %t0, align 1
  %v1 = call i8 (i8) @arm64.bump(i8 zeroext %v0)
  store i8 %v1, ptr %t2, align 1
  %v2 = load i8, ptr %t2, align 1
  %v3 = load i8, ptr %t1, align 1
  %v4 = call i32 (i8, i8, i16) @widen(i8 zeroext %v2, i8 signext %v3, i16 zeroext 7)
  store i32 %v4, ptr %t3, align 4
  %v5 = load i32, ptr %t3, align 4
  ret i32 %v5
}

define internal noundef i64 @arm64.last(i8 zeroext noundef %p0, i16 signext noundef %p1, i64 noundef %p2, i64 noundef %p3, i64 noundef %p4, i64 noundef %p5, i64 noundef %p6, i64 noundef %p7, i8 signext noundef %p8, i32 noundef %p9, i8 zeroext noundef %p10) #0 {
b0:
  %t0 = alloca i8, align 1
  %t1 = alloca i16, align 2
  %t2 = alloca i64, align 8
  %t3 = alloca i64, align 8
  %t4 = alloca i64, align 8
  %t5 = alloca i64, align 8
  %t6 = alloca i64, align 8
  %t7 = alloca i64, align 8
  %t8 = alloca i8, align 1
  %t9 = alloca i32, align 4
  %t10 = alloca i8, align 1
  %t11 = alloca i64, align 8
  %t12 = alloca i64, align 8
  %t13 = alloca i64, align 8
  %t14 = alloca i64, align 8
  store i8 %p0, ptr %t0, align 1
  store i16 %p1, ptr %t1, align 2
  store i64 %p2, ptr %t2, align 8
  store i64 %p3, ptr %t3, align 8
  store i64 %p4, ptr %t4, align 8
  store i64 %p5, ptr %t5, align 8
  store i64 %p6, ptr %t6, align 8
  store i64 %p7, ptr %t7, align 8
  store i8 %p8, ptr %t8, align 1
  store i32 %p9, ptr %t9, align 4
  store i8 %p10, ptr %t10, align 1
  %v0 = load i32, ptr %t9, align 4
  %v1 = sext i32 %v0 to i64
  store i64 %v1, ptr %t11, align 8
  %v2 = load i64, ptr %t2, align 8
  %v3 = load i64, ptr %t11, align 8
  %v4 = add i64 %v2, %v3
  store i64 %v4, ptr %t12, align 8
  %v5 = load i8, ptr %t10, align 1
  %v6 = zext i8 %v5 to i64
  store i64 %v6, ptr %t13, align 8
  %v7 = load i64, ptr %t12, align 8
  %v8 = load i64, ptr %t13, align 8
  %v9 = add i64 %v7, %v8
  store i64 %v9, ptr %t14, align 8
  %v10 = load i64, ptr %t14, align 8
  ret i64 %v10
}

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @widen(i8 zeroext noundef, i8 signext noundef, i16 zeroext noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
