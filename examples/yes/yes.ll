; ModuleID = 'yes.bc'
source_filename = "yes.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }

@.str = private unnamed_addr constant [2 x i8] c" \00", align 1
@.str.1 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@.str.2 = private unnamed_addr constant [3 x i8] c"y\0A\00", align 1
@stdout = external global %struct._IO_FILE*, align 8

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main(i32 %argc, i8** %argv) #0 !dbg !10 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %output = alloca i8*, align 8
  %output_size = alloca i32, align 4
  %output_len = alloca i32, align 4
  %needed_size = alloca i32, align 4
  %i = alloca i32, align 4
  %i13 = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %argc.addr, metadata !15, metadata !16), !dbg !17
  store i8** %argv, i8*** %argv.addr, align 8
  call void @llvm.dbg.declare(metadata i8*** %argv.addr, metadata !18, metadata !16), !dbg !19
  call void @llvm.dbg.declare(metadata i8** %output, metadata !20, metadata !16), !dbg !21
  call void @llvm.dbg.declare(metadata i32* %output_size, metadata !22, metadata !16), !dbg !23
  call void @llvm.dbg.declare(metadata i32* %output_len, metadata !24, metadata !16), !dbg !25
  %0 = load i32, i32* %argc.addr, align 4, !dbg !26
  %cmp = icmp sgt i32 %0, 1, !dbg !28
  br i1 %cmp, label %if.then, label %if.else, !dbg !29

if.then:                                          ; preds = %entry
  call void @llvm.dbg.declare(metadata i32* %needed_size, metadata !30, metadata !16), !dbg !32
  %1 = load i8**, i8*** %argv.addr, align 8, !dbg !33
  %arrayidx = getelementptr inbounds i8*, i8** %1, i64 1, !dbg !33
  %2 = load i8*, i8** %arrayidx, align 8, !dbg !33
  %call = call i64 @strlen(i8* %2) #5, !dbg !34
  %add = add i64 2, %call, !dbg !35
  %conv = trunc i64 %add to i32, !dbg !36
  store i32 %conv, i32* %needed_size, align 4, !dbg !32
  call void @llvm.dbg.declare(metadata i32* %i, metadata !37, metadata !16), !dbg !39
  store i32 2, i32* %i, align 4, !dbg !39
  br label %for.cond, !dbg !40

for.cond:                                         ; preds = %for.inc, %if.then
  %3 = load i32, i32* %i, align 4, !dbg !41
  %4 = load i32, i32* %argc.addr, align 4, !dbg !43
  %cmp1 = icmp slt i32 %3, %4, !dbg !44
  br i1 %cmp1, label %for.body, label %for.end, !dbg !45

for.body:                                         ; preds = %for.cond
  %5 = load i8**, i8*** %argv.addr, align 8, !dbg !46
  %6 = load i32, i32* %i, align 4, !dbg !48
  %idxprom = sext i32 %6 to i64, !dbg !46
  %arrayidx3 = getelementptr inbounds i8*, i8** %5, i64 %idxprom, !dbg !46
  %7 = load i8*, i8** %arrayidx3, align 8, !dbg !46
  %call4 = call i64 @strlen(i8* %7) #5, !dbg !49
  %add5 = add i64 1, %call4, !dbg !50
  %8 = load i32, i32* %needed_size, align 4, !dbg !51
  %conv6 = sext i32 %8 to i64, !dbg !51
  %add7 = add i64 %conv6, %add5, !dbg !51
  %conv8 = trunc i64 %add7 to i32, !dbg !51
  store i32 %conv8, i32* %needed_size, align 4, !dbg !51
  br label %for.inc, !dbg !52

for.inc:                                          ; preds = %for.body
  %9 = load i32, i32* %i, align 4, !dbg !53
  %inc = add nsw i32 %9, 1, !dbg !53
  store i32 %inc, i32* %i, align 4, !dbg !53
  br label %for.cond, !dbg !54, !llvm.loop !55

for.end:                                          ; preds = %for.cond
  %10 = load i32, i32* %needed_size, align 4, !dbg !57
  %conv9 = sext i32 %10 to i64, !dbg !57
  %call10 = call noalias i8* @malloc(i64 %conv9) #6, !dbg !58
  store i8* %call10, i8** %output, align 8, !dbg !59
  %11 = load i8*, i8** %output, align 8, !dbg !60
  %12 = load i8**, i8*** %argv.addr, align 8, !dbg !61
  %arrayidx11 = getelementptr inbounds i8*, i8** %12, i64 1, !dbg !61
  %13 = load i8*, i8** %arrayidx11, align 8, !dbg !61
  %call12 = call i8* @strcat(i8* %11, i8* %13) #6, !dbg !62
  call void @llvm.dbg.declare(metadata i32* %i13, metadata !63, metadata !16), !dbg !65
  store i32 2, i32* %i13, align 4, !dbg !65
  br label %for.cond14, !dbg !66

for.cond14:                                       ; preds = %for.inc22, %for.end
  %14 = load i32, i32* %i13, align 4, !dbg !67
  %15 = load i32, i32* %argc.addr, align 4, !dbg !69
  %cmp15 = icmp slt i32 %14, %15, !dbg !70
  br i1 %cmp15, label %for.body17, label %for.end24, !dbg !71

for.body17:                                       ; preds = %for.cond14
  %16 = load i8*, i8** %output, align 8, !dbg !72
  %call18 = call i8* @strcat(i8* %16, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i32 0, i32 0)) #6, !dbg !74
  %17 = load i8*, i8** %output, align 8, !dbg !75
  %18 = load i8**, i8*** %argv.addr, align 8, !dbg !76
  %19 = load i32, i32* %i13, align 4, !dbg !77
  %idxprom19 = sext i32 %19 to i64, !dbg !76
  %arrayidx20 = getelementptr inbounds i8*, i8** %18, i64 %idxprom19, !dbg !76
  %20 = load i8*, i8** %arrayidx20, align 8, !dbg !76
  %call21 = call i8* @strcat(i8* %17, i8* %20) #6, !dbg !78
  br label %for.inc22, !dbg !79

for.inc22:                                        ; preds = %for.body17
  %21 = load i32, i32* %i13, align 4, !dbg !80
  %inc23 = add nsw i32 %21, 1, !dbg !80
  store i32 %inc23, i32* %i13, align 4, !dbg !80
  br label %for.cond14, !dbg !81, !llvm.loop !82

for.end24:                                        ; preds = %for.cond14
  %22 = load i8*, i8** %output, align 8, !dbg !84
  %call25 = call i8* @strcat(i8* %22, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.1, i32 0, i32 0)) #6, !dbg !85
  %23 = load i8*, i8** %output, align 8, !dbg !86
  %call26 = call i64 @strlen(i8* %23) #5, !dbg !87
  %conv27 = trunc i64 %call26 to i32, !dbg !87
  store i32 %conv27, i32* %output_len, align 4, !dbg !88
  br label %if.end, !dbg !89

if.else:                                          ; preds = %entry
  store i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i32 0, i32 0), i8** %output, align 8, !dbg !90
  store i32 2, i32* %output_len, align 4, !dbg !92
  br label %if.end

if.end:                                           ; preds = %if.else, %for.end24
  br label %for.cond28, !dbg !93

for.cond28:                                       ; preds = %for.cond28, %if.end
  %24 = load i8*, i8** %output, align 8, !dbg !94
  %25 = load i32, i32* %output_len, align 4, !dbg !97
  %conv29 = sext i32 %25 to i64, !dbg !97
  %26 = load %struct._IO_FILE*, %struct._IO_FILE** @stdout, align 8, !dbg !98
  %call30 = call i64 @fwrite(i8* %24, i64 1, i64 %conv29, %struct._IO_FILE* %26), !dbg !99
  br label %for.cond28, !dbg !100, !llvm.loop !101

return:                                           ; No predecessors!
  %27 = load i32, i32* %retval, align 4, !dbg !104
  ret i32 %27, !dbg !104
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readonly
declare i64 @strlen(i8*) #2

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) #3

; Function Attrs: nounwind
declare i8* @strcat(i8*, i8*) #3

declare i64 @fwrite(i8*, i64, i64, %struct._IO_FILE*) #4

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }
attributes #2 = { nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind readonly }
attributes #6 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 5.0.1 (tags/RELEASE_500/final)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3)
!1 = !DIFile(filename: "yes.c", directory: "/mnt/Documents/work/university/muni/msc/thesis/APEX/examples/experiments/yes")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{i32 2, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{!"clang version 5.0.1 (tags/RELEASE_500/final)"}
!10 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 10, type: !11, isLocal: false, isDefinition: true, scopeLine: 10, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !13, !14}
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!15 = !DILocalVariable(name: "argc", arg: 1, scope: !10, file: !1, line: 10, type: !13)
!16 = !DIExpression()
!17 = !DILocation(line: 10, column: 14, scope: !10)
!18 = !DILocalVariable(name: "argv", arg: 2, scope: !10, file: !1, line: 10, type: !14)
!19 = !DILocation(line: 10, column: 26, scope: !10)
!20 = !DILocalVariable(name: "output", scope: !10, file: !1, line: 11, type: !4)
!21 = !DILocation(line: 11, column: 8, scope: !10)
!22 = !DILocalVariable(name: "output_size", scope: !10, file: !1, line: 12, type: !13)
!23 = !DILocation(line: 12, column: 6, scope: !10)
!24 = !DILocalVariable(name: "output_len", scope: !10, file: !1, line: 13, type: !13)
!25 = !DILocation(line: 13, column: 6, scope: !10)
!26 = !DILocation(line: 15, column: 6, scope: !27)
!27 = distinct !DILexicalBlock(scope: !10, file: !1, line: 15, column: 6)
!28 = !DILocation(line: 15, column: 11, scope: !27)
!29 = !DILocation(line: 15, column: 6, scope: !10)
!30 = !DILocalVariable(name: "needed_size", scope: !31, file: !1, line: 17, type: !13)
!31 = distinct !DILexicalBlock(scope: !27, file: !1, line: 15, column: 16)
!32 = !DILocation(line: 17, column: 7, scope: !31)
!33 = !DILocation(line: 17, column: 32, scope: !31)
!34 = !DILocation(line: 17, column: 25, scope: !31)
!35 = !DILocation(line: 17, column: 23, scope: !31)
!36 = !DILocation(line: 17, column: 21, scope: !31)
!37 = !DILocalVariable(name: "i", scope: !38, file: !1, line: 18, type: !13)
!38 = distinct !DILexicalBlock(scope: !31, file: !1, line: 18, column: 3)
!39 = !DILocation(line: 18, column: 12, scope: !38)
!40 = !DILocation(line: 18, column: 8, scope: !38)
!41 = !DILocation(line: 18, column: 19, scope: !42)
!42 = distinct !DILexicalBlock(scope: !38, file: !1, line: 18, column: 3)
!43 = !DILocation(line: 18, column: 23, scope: !42)
!44 = !DILocation(line: 18, column: 21, scope: !42)
!45 = !DILocation(line: 18, column: 3, scope: !38)
!46 = !DILocation(line: 19, column: 30, scope: !47)
!47 = distinct !DILexicalBlock(scope: !42, file: !1, line: 18, column: 34)
!48 = !DILocation(line: 19, column: 35, scope: !47)
!49 = !DILocation(line: 19, column: 23, scope: !47)
!50 = !DILocation(line: 19, column: 21, scope: !47)
!51 = !DILocation(line: 19, column: 16, scope: !47)
!52 = !DILocation(line: 20, column: 3, scope: !47)
!53 = !DILocation(line: 18, column: 30, scope: !42)
!54 = !DILocation(line: 18, column: 3, scope: !42)
!55 = distinct !{!55, !45, !56}
!56 = !DILocation(line: 20, column: 3, scope: !38)
!57 = !DILocation(line: 22, column: 27, scope: !31)
!58 = !DILocation(line: 22, column: 20, scope: !31)
!59 = !DILocation(line: 22, column: 10, scope: !31)
!60 = !DILocation(line: 25, column: 10, scope: !31)
!61 = !DILocation(line: 25, column: 18, scope: !31)
!62 = !DILocation(line: 25, column: 3, scope: !31)
!63 = !DILocalVariable(name: "i", scope: !64, file: !1, line: 26, type: !13)
!64 = distinct !DILexicalBlock(scope: !31, file: !1, line: 26, column: 3)
!65 = !DILocation(line: 26, column: 12, scope: !64)
!66 = !DILocation(line: 26, column: 8, scope: !64)
!67 = !DILocation(line: 26, column: 19, scope: !68)
!68 = distinct !DILexicalBlock(scope: !64, file: !1, line: 26, column: 3)
!69 = !DILocation(line: 26, column: 23, scope: !68)
!70 = !DILocation(line: 26, column: 21, scope: !68)
!71 = !DILocation(line: 26, column: 3, scope: !64)
!72 = !DILocation(line: 27, column: 11, scope: !73)
!73 = distinct !DILexicalBlock(scope: !68, file: !1, line: 26, column: 34)
!74 = !DILocation(line: 27, column: 4, scope: !73)
!75 = !DILocation(line: 28, column: 11, scope: !73)
!76 = !DILocation(line: 28, column: 19, scope: !73)
!77 = !DILocation(line: 28, column: 24, scope: !73)
!78 = !DILocation(line: 28, column: 4, scope: !73)
!79 = !DILocation(line: 29, column: 3, scope: !73)
!80 = !DILocation(line: 26, column: 30, scope: !68)
!81 = !DILocation(line: 26, column: 3, scope: !68)
!82 = distinct !{!82, !71, !83}
!83 = !DILocation(line: 29, column: 3, scope: !64)
!84 = !DILocation(line: 30, column: 10, scope: !31)
!85 = !DILocation(line: 30, column: 3, scope: !31)
!86 = !DILocation(line: 31, column: 23, scope: !31)
!87 = !DILocation(line: 31, column: 16, scope: !31)
!88 = !DILocation(line: 31, column: 14, scope: !31)
!89 = !DILocation(line: 32, column: 2, scope: !31)
!90 = !DILocation(line: 33, column: 10, scope: !91)
!91 = distinct !DILexicalBlock(scope: !27, file: !1, line: 32, column: 9)
!92 = !DILocation(line: 34, column: 14, scope: !91)
!93 = !DILocation(line: 38, column: 2, scope: !10)
!94 = !DILocation(line: 39, column: 10, scope: !95)
!95 = distinct !DILexicalBlock(scope: !96, file: !1, line: 38, column: 2)
!96 = distinct !DILexicalBlock(scope: !10, file: !1, line: 38, column: 2)
!97 = !DILocation(line: 39, column: 21, scope: !95)
!98 = !DILocation(line: 39, column: 33, scope: !95)
!99 = !DILocation(line: 39, column: 3, scope: !95)
!100 = !DILocation(line: 38, column: 2, scope: !95)
!101 = distinct !{!101, !102, !103}
!102 = !DILocation(line: 38, column: 2, scope: !96)
!103 = !DILocation(line: 39, column: 39, scope: !96)
!104 = !DILocation(line: 40, column: 1, scope: !10)
