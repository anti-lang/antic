source_filename = "letters"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macos11.0"

define internal noundef i64 @letters.main() #0 {
b0:
  %t0 = alloca i64, align 8
  %t1 = alloca i8, align 1
  %t2 = alloca i32, align 4
  %t3 = alloca i32, align 4
  %t4 = alloca i32, align 4
  store i64 0, ptr %t0, align 8
  br label %b1

b1:
  %v0 = load i64, ptr %t0, align 8
  %v1 = icmp slt i64 %v0, 3
  %v2 = zext i1 %v1 to i8
  store i8 %v2, ptr %t1, align 1
  %v3 = load i8, ptr %t1, align 1
  %v4 = trunc i8 %v3 to i1
  br i1 %v4, label %b2, label %b3

b2:
  %v5 = load i64, ptr %t0, align 8
  %v6 = trunc i64 %v5 to i32
  store i32 %v6, ptr %t2, align 4
  %v7 = load i32, ptr %t2, align 4
  %v8 = add i32 %v7, 65
  store i32 %v8, ptr %t3, align 4
  %v9 = load i32, ptr %t3, align 4
  %v10 = call i32 (i32) @putchar(i32 %v9)
  store i32 %v10, ptr %t4, align 4
  %v11 = load i64, ptr %t0, align 8
  %v12 = add i64 %v11, 1
  store i64 %v12, ptr %t0, align 8
  br label %b1

b3:
  %v13 = load i64, ptr %t0, align 8
  ret i64 %v13
}

@anti.rt.main = alias i64 (), ptr @letters.main

@anti_rt_slots = dso_local constant <{ [24 x i8] }> zeroinitializer, align 8
@anti_rt_injectable = dso_local constant <{ [16 x i8] }> zeroinitializer, align 8

declare noundef i32 @putchar(i32 noundef) #1

attributes #0 = { nounwind "frame-pointer"="non-leaf" "target-cpu"="generic" "target-features"="+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv" "tune-cpu"="apple-m1" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"antic VERSION"}
