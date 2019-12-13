
echo "=== compile for profiling data ==="
clang ${1}.c -o ${1}.ll -emit-llvm -S -lm  > /dev/null 2>&1

opt -pgo-instr-gen -instrprof ${1}.ll -o ${1}.prof.bc

# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.prof.bc -o ${1}.prof -lm

# Collect profiling data
echo "=== running profiling generation code ==="
time ./${1}.prof
llvm-profdata merge -o ${1}.profdata default.profraw



clang ${1}.c -o ${1}cilk.ll -emit-llvm -S -O3 -lm -DFIXUP  > /dev/null 2>&1

opt -dot-cfg ${1}cilk.ll > /dev/null 2>&1
cat .main.dot | dot -Tpdf > ${1}.pdf 
rm .main.dot

opt -o ${1}.opt.ll -pgo-instr-use -pgo-test-profile-file=${1}.profdata -load ../new/tapir/build/lib/LLVMHello.so -hello < ${1}cilk.ll > /dev/null 
# opt -load ../new/tapir/build/lib/LLVMHello.so -hello ${1}.ll -o ${1}.opt.ll -S
# opt ${1}.ll -loop-simplify -o ${1}.opt.ll -S

opt -dot-cfg ${1}.opt.ll > /dev/null 2>&1
cat .main.dot | dot -Tpdf > ${1}.opt.pdf 
rm .main.dot

# clang ${1}.opt.ll -o ${1}.opt.ll -O3 -lm -emit-llvm -S

# opt -dot-cfg ${1}.opt.ll > /dev/null 2>&1
# cat cfg.main.dot | dot -Tpdf > ${1}.opto3.pdf 
# rm cfg.main.dot

clang ${1}.opt.ll -o ${1} -fcilkplus -lm -O3

echo "=== run compiled serial code ==="
# opt -dot-cfg ${1} > /dev/null 2>&1
# cat cfg.main.dot | dot -Tpdf > ${1}.final.pdf 
# rm cfg.main.dot
time ./${1}

