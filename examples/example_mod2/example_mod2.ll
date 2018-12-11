; ModuleID = 'example_mod2.bc'
source_filename = "example_mod2.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define i32 @foo(i32 %n) #0 !dbg !7 {
entry:
  %n.addr = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 %n, i32* %n.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %n.addr, metadata !11, metadata !12), !dbg !13
  call void @llvm.dbg.declare(metadata i32* %x, metadata !14, metadata !12), !dbg !15
  %0 = load i32, i32* %n.addr, align 4, !dbg !16
  %add = add nsw i32 %0, 10, !dbg !17
  store i32 %add, i32* %x, align 4, !dbg !15
  %1 = load i32, i32* %x, align 4, !dbg !18
  ret i32 %1, !dbg !19
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: noinline nounwind optnone uwtable
define i32 @qux() #0 !dbg !20 {
entry:
  ret i32 42, !dbg !23
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @bar() #0 !dbg !24 {
entry:
  %y = alloca i32, align 4
  call void @llvm.dbg.declare(metadata i32* %y, metadata !25, metadata !12), !dbg !26
  %call = call i32 @qux(), !dbg !27
  store i32 %call, i32* %y, align 4, !dbg !26
  %0 = load i32, i32* %y, align 4, !dbg !28
  ret i32 %0, !dbg !29
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main() #0 !dbg !30 {
entry:
  %retval = alloca i32, align 4
  %some_int = alloca i32, align 4
  %foo_result = alloca i32, align 4
  %n = alloca i32, align 4
  %bar_result = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  call void @llvm.dbg.declare(metadata i32* %some_int, metadata !31, metadata !12), !dbg !32
  store i32 10, i32* %some_int, align 4, !dbg !32
  call void @llvm.dbg.declare(metadata i32* %foo_result, metadata !33, metadata !12), !dbg !34
  %0 = load i32, i32* %some_int, align 4, !dbg !35
  %call = call i32 @foo(i32 %0), !dbg !36
  store i32 %call, i32* %foo_result, align 4, !dbg !34
  call void @llvm.dbg.declare(metadata i32* %n, metadata !37, metadata !12), !dbg !38
  store i32 10, i32* %n, align 4, !dbg !38
  %1 = load i32, i32* %n, align 4, !dbg !39
  %cmp = icmp slt i32 %1, 42, !dbg !41
  br i1 %cmp, label %if.then, label %if.end, !dbg !42

if.then:                                          ; preds = %entry
  call void @llvm.dbg.declare(metadata i32* %bar_result, metadata !43, metadata !12), !dbg !45
  %call1 = call i32 @bar(), !dbg !46
  store i32 %call1, i32* %bar_result, align 4, !dbg !45
  br label %if.end, !dbg !47

if.end:                                           ; preds = %if.then, %entry
  ret i32 0, !dbg !48
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 5.0.1 (tags/RELEASE_500/final)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
!1 = !DIFile(filename: "example_mod2.c", directory: "/mnt/Documents/work/university/muni/msc/thesis/APEX/examples/experiments/example_mod2")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 5.0.1 (tags/RELEASE_500/final)"}
!7 = distinct !DISubprogram(name: "foo", scope: !1, file: !1, line: 1, type: !8, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DILocalVariable(name: "n", arg: 1, scope: !7, file: !1, line: 1, type: !10)
!12 = !DIExpression()
!13 = !DILocation(line: 1, column: 13, scope: !7)
!14 = !DILocalVariable(name: "x", scope: !7, file: !1, line: 2, type: !10)
!15 = !DILocation(line: 2, column: 9, scope: !7)
!16 = !DILocation(line: 2, column: 13, scope: !7)
!17 = !DILocation(line: 2, column: 15, scope: !7)
!18 = !DILocation(line: 3, column: 12, scope: !7)
!19 = !DILocation(line: 3, column: 5, scope: !7)
!20 = distinct !DISubprogram(name: "qux", scope: !1, file: !1, line: 6, type: !21, isLocal: false, isDefinition: true, scopeLine: 6, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!21 = !DISubroutineType(types: !22)
!22 = !{!10}
!23 = !DILocation(line: 7, column: 2, scope: !20)
!24 = distinct !DISubprogram(name: "bar", scope: !1, file: !1, line: 10, type: !21, isLocal: false, isDefinition: true, scopeLine: 10, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!25 = !DILocalVariable(name: "y", scope: !24, file: !1, line: 11, type: !10)
!26 = !DILocation(line: 11, column: 9, scope: !24)
!27 = !DILocation(line: 11, column: 13, scope: !24)
!28 = !DILocation(line: 12, column: 12, scope: !24)
!29 = !DILocation(line: 12, column: 5, scope: !24)
!30 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 15, type: !21, isLocal: false, isDefinition: true, scopeLine: 15, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!31 = !DILocalVariable(name: "some_int", scope: !30, file: !1, line: 16, type: !10)
!32 = !DILocation(line: 16, column: 7, scope: !30)
!33 = !DILocalVariable(name: "foo_result", scope: !30, file: !1, line: 17, type: !10)
!34 = !DILocation(line: 17, column: 7, scope: !30)
!35 = !DILocation(line: 17, column: 24, scope: !30)
!36 = !DILocation(line: 17, column: 20, scope: !30)
!37 = !DILocalVariable(name: "n", scope: !30, file: !1, line: 18, type: !10)
!38 = !DILocation(line: 18, column: 7, scope: !30)
!39 = !DILocation(line: 19, column: 7, scope: !40)
!40 = distinct !DILexicalBlock(scope: !30, file: !1, line: 19, column: 7)
!41 = !DILocation(line: 19, column: 9, scope: !40)
!42 = !DILocation(line: 19, column: 7, scope: !30)
!43 = !DILocalVariable(name: "bar_result", scope: !44, file: !1, line: 20, type: !10)
!44 = distinct !DILexicalBlock(scope: !40, file: !1, line: 19, column: 15)
!45 = !DILocation(line: 20, column: 7, scope: !44)
!46 = !DILocation(line: 20, column: 20, scope: !44)
!47 = !DILocation(line: 21, column: 2, scope: !44)
!48 = !DILocation(line: 22, column: 5, scope: !30)
