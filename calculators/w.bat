set compilerName=%userprofile%\emsdk\emsdk\upstream\emscripten\node_modules\google-closure-compiler-java\compiler.jar
set compilerOptions2=--compilation_level ADVANCED_OPTIMIZATIONS --isolation_mode IIFE --externs=custom-externs.js
set commonOptions=--no-entry -Os -Wall -s WASM=0 -s TEXTDECODER=0 -s MIN_IE_VERSION=11 -s ASSERTIONS=0 -s NO_FILESYSTEM=1 -s WASM_ASYNC_COMPILATION=0 --pre-js preGraphics.js --closure 1 --memory-init-file 0
set wasmCommon=--no-entry -Os -Wall -s WASM=1 -D_USING64BITS_ -s ASSERTIONS=0 -s NO_FILESYSTEM=1 --js-library lib.js --pre-js pre.js
rem del *.wasm
rem del *00*js

rem ==================== GENERATION OF ASM.JS ===============================
cmd /c emcc ulam.c isprime.c graphics.c copyStr.c -s EXPORTED_FUNCTIONS="['_moveGraphic', '_drawPartialGraphic', '_nbrChanged', '_getInformation', '_getPixels']" -s TOTAL_MEMORY=33554432 %commonOptions% -o ulamW.js
if errorlevel 1 goto end
cmd /c emcc gausspr.c isprime.c graphics.c -s EXPORTED_FUNCTIONS="['_moveGraphic', '_drawPartialGraphic', '_nbrChanged', '_getInformation', '_getPixels']" -s TOTAL_MEMORY=33554432 %commonOptions% -o gaussprW.js
if errorlevel 1 goto end

rem ===================== GENERATION OF WASM ================================
cmd /c emcc %wasmCommon% ulam.c isprime.c graphics.c copyStr.c -s EXPORTED_FUNCTIONS="['_moveGraphic', '_drawPartialGraphic', '_nbrChanged', '_getInformation', '_getPixels']" -s TOTAL_MEMORY=33554432 -o ulam.wasm
if errorlevel 1 goto end
cmd /c emcc %wasmCommon% gausspr.c isprime.c graphics.c -s EXPORTED_FUNCTIONS="['_moveGraphic', '_drawPartialGraphic', '_nbrChanged', '_getInformation', '_getPixels']" -s TOTAL_MEMORY=33554432 -o gausspr.wasm
if errorlevel 1 goto end

copy /b ulam.js + common.js + strings.js + commonGraphics.js ulamA.js
perl generateTempJS.pl ulamA.js ulamW.js ulamT.js moveGraphic drawPartialGraphic nbrChanged getInformation getPixels
del ulamA.js
java -jar %compilerName% %compilerOptions2% --js ulamT.js --js_output_file ulamU.js
copy ulamU.js ulamV.js
copy ULAM.HTM toweb
perl replaceEmbeddedJS.pl 0000 toweb\ULAM.HTM ulamV.js ulam.wasm
copy EULAM.HTM toweb
perl replaceEmbeddedJS.pl 0000 toweb\EULAM.HTM ulamU.js ulam.wasm

copy /b gausspr.js + common.js + strings.js + commonGraphics.js gaussprA.js
perl generateTempJS.pl gaussprA.js gaussprW.js gaussprT.js moveGraphic drawPartialGraphic nbrChanged getInformation getPixels
del gaussprA.js
java -jar %compilerName% %compilerOptions2% --js gaussprT.js --js_output_file gaussprU.js
copy gaussprU.js gaussprV.js
copy GAUSSPR.HTM toweb
perl replaceEmbeddedJS.pl 0000 toweb\GAUSSPR.HTM gaussprV.js gausspr.wasm
copy PRGAUSS.HTM toweb
perl replaceEmbeddedJS.pl 0000 toweb\PRGAUSS.HTM gaussprU.js gausspr.wasm

:end