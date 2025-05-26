cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build
LLVM_PROFILE_FILE="default.profraw" ./bin/tests
llvm-profdata merge -sparse default.profraw -o default.profdata
llvm-cov show ./bin/tests \
  -instr-profile=default.profdata \
  --ignore-filename-regex='(build/_deps/|tests/)' \
  -format=html -output-dir=coverage_html
open coverage_html/index.html