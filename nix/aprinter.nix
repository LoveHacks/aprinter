{ stdenv, writeText, bash, gccAvrAtmel, gcc-arm-embedded, asf, teensyCores, buildName
, boardName, mainText
, assertionsEnabled ? false
, eventLoopBenchmarkEnabled ? false
, detectOverloadEnabled ? false
, forceUartSerial ? false
}:

let
    boardDefinitions = import ./boards.nix;
    
    board = builtins.getAttr boardName boardDefinitions;
    
    targetVars = {
        PLATFORM = board.platform;
    } // (if board.platform == "sam3x" then {
        USE_USB_SERIAL = if forceUartSerial then "0" else "1";
    } else {}) // board.targetVars;
    
    targetVarsText = stdenv.lib.concatStrings (
        stdenv.lib.mapAttrsToList (name: value: "    ${name}=${value}\n") targetVars
    );
    
    isAvr = board.platform == "avr";
    
    isArm = builtins.elem board.platform [ "sam3x" "teensy" "stm" ];
    
    needAsf = board.platform == "sam3x";
    
    needTeensyCores = board.platform == "teensy";
    
    targetFile = writeText "aprinter-nixbuild.sh" ''
        ${stdenv.lib.optionalString isAvr "CUSTOM_AVR_GCC=${gccAvrAtmel}/bin/avr-"}
        ${stdenv.lib.optionalString isArm "CUSTOM_ARM_GCC=${gcc-arm-embedded}/bin/arm-none-eabi-"}
        ${stdenv.lib.optionalString needAsf "CUSTOM_ASF=${asf}"}
        ${stdenv.lib.optionalString needTeensyCores "CUSTOM_TEENSY_CORES=${teensyCores}"}
        
        TARGETS+=( "nixbuild" )
        target_nixbuild() {
        ${targetVarsText}}
    '';
    
    mainFile = writeText "aprinter-main.cpp" mainText;
    
    compileFlags = stdenv.lib.concatStringsSep " " [
        (stdenv.lib.optionalString assertionsEnabled "-DAMBROLIB_ASSERTIONS")
        (stdenv.lib.optionalString eventLoopBenchmarkEnabled "-DEVENTLOOP_BENCHMARK")
        (stdenv.lib.optionalString detectOverloadEnabled "-DAXISDRIVER_DETECT_OVERLOAD")
    ];
    
in

assert isAvr -> gccAvrAtmel != null;
assert isArm -> gcc-arm-embedded != null;
assert needAsf -> asf != null;
assert needTeensyCores -> teensyCores != null;

stdenv.mkDerivation rec {
    aprinterBuildName = buildName;
    
    name = "aprinter-${buildName}";
    
    src = stdenv.lib.cleanSource ./..;
    
    configurePhase = ''
        rm config/*
        ln -s ${targetFile} config/nixbuild.sh
        ln -s ${mainFile} main/aprinter-nixbuild.cpp
    '';
    
    buildPhase = ''
        echo "Compile flags: ${compileFlags}"
        CFLAGS="${compileFlags}" \
        CXXFLAGS="${compileFlags}" \
        ${bash}/bin/bash ./build.sh nixbuild build
    '';
    
    installPhase = ''
        mkdir -p $out
        cp build/aprinter-nixbuild.* $out/
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
