; ModuleID = 'yes_nodbg.bc'
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
define i32 @main(i32 %argc, i8** %argv) #0 {
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
  store i8** %argv, i8*** %argv.addr, align 8
  %0 = load i32, i32* %argc.addr, align 4
  %cmp = icmp sgt i32 %0, 1
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %1 = load i8**, i8*** %argv.addr, align 8
  %arrayidx = getelementptr inbounds i8*, i8** %1, i64 1
  %2 = load i8*, i8** %arrayidx, align 8
  %call = call i64 @strlen(i8* %2) #4
  %add = add i64 2, %call
  %conv = trunc i64 %add to i32
  store i32 %conv, i32* %needed_size, align 4
  store i32 2, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %if.then
  %3 = load i32, i32* %i, align 4
  %4 = load i32, i32* %argc.addr, align 4
  %cmp1 = icmp slt i32 %3, %4
  br i1 %cmp1, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %5 = load i8**, i8*** %argv.addr, align 8
  %6 = load i32, i32* %i, align 4
  %idxprom = sext i32 %6 to i64
  %arrayidx3 = getelementptr inbounds i8*, i8** %5, i64 %idxprom
  %7 = load i8*, i8** %arrayidx3, align 8
  %call4 = call i64 @strlen(i8* %7) #4
  %add5 = add i64 1, %call4
  %8 = load i32, i32* %needed_size, align 4
  %conv6 = sext i32 %8 to i64
  %add7 = add i64 %conv6, %add5
  %conv8 = trunc i64 %add7 to i32
  store i32 %conv8, i32* %needed_size, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %9 = load i32, i32* %i, align 4
  %inc = add nsw i32 %9, 1
  store i32 %inc, i32* %i, align 4
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %10 = load i32, i32* %needed_size, align 4
  %conv9 = sext i32 %10 to i64
  %call10 = call noalias i8* @malloc(i64 %conv9) #5
  store i8* %call10, i8** %output, align 8
  %11 = load i8*, i8** %output, align 8
  %12 = load i8**, i8*** %argv.addr, align 8
  %arrayidx11 = getelementptr inbounds i8*, i8** %12, i64 1
  %13 = load i8*, i8** %arrayidx11, align 8
  %call12 = call i8* @strcat(i8* %11, i8* %13) #5
  store i32 2, i32* %i13, align 4
  br label %for.cond14

for.cond14:                                       ; preds = %for.inc22, %for.end
  %14 = load i32, i32* %i13, align 4
  %15 = load i32, i32* %argc.addr, align 4
  %cmp15 = icmp slt i32 %14, %15
  br i1 %cmp15, label %for.body17, label %for.end24

for.body17:                                       ; preds = %for.cond14
  %16 = load i8*, i8** %output, align 8
  %call18 = call i8* @strcat(i8* %16, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i32 0, i32 0)) #5
  %17 = load i8*, i8** %output, align 8
  %18 = load i8**, i8*** %argv.addr, align 8
  %19 = load i32, i32* %i13, align 4
  %idxprom19 = sext i32 %19 to i64
  %arrayidx20 = getelementptr inbounds i8*, i8** %18, i64 %idxprom19
  %20 = load i8*, i8** %arrayidx20, align 8
  %call21 = call i8* @strcat(i8* %17, i8* %20) #5
  br label %for.inc22

for.inc22:                                        ; preds = %for.body17
  %21 = load i32, i32* %i13, align 4
  %inc23 = add nsw i32 %21, 1
  store i32 %inc23, i32* %i13, align 4
  br label %for.cond14

for.end24:                                        ; preds = %for.cond14
  %22 = load i8*, i8** %output, align 8
  %call25 = call i8* @strcat(i8* %22, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.1, i32 0, i32 0)) #5
  %23 = load i8*, i8** %output, align 8
  %call26 = call i64 @strlen(i8* %23) #4
  %conv27 = trunc i64 %call26 to i32
  store i32 %conv27, i32* %output_len, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  store i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i32 0, i32 0), i8** %output, align 8
  store i32 2, i32* %output_len, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %for.end24
  br label %for.cond28

for.cond28:                                       ; preds = %for.cond28, %if.end
  %24 = load i8*, i8** %output, align 8
  %25 = load i32, i32* %output_len, align 4
  %conv29 = sext i32 %25 to i64
  %26 = load %struct._IO_FILE*, %struct._IO_FILE** @stdout, align 8
  %call30 = call i64 @fwrite(i8* %24, i64 1, i64 %conv29, %struct._IO_FILE* %26)
  br label %for.cond28

return:                                           ; No predecessors!
  %27 = load i32, i32* %retval, align 4
  ret i32 %27
}

; Function Attrs: nounwind readonly
declare i64 @strlen(i8*) #1

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) #2

; Function Attrs: nounwind
declare i8* @strcat(i8*, i8*) #2

declare i64 @fwrite(i8*, i64, i64, %struct._IO_FILE*) #3

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind readonly }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.1 (tags/RELEASE_500/final)"}
