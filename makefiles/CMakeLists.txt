cmake_minimum_required(VERSION 3.6)

project(bdelta)

option(BDELTA_PGO      "build using PGO instead of PGI (MSVC only)" "OFF")
option(GCC32           "build 32 bit executable instead of 64 bit (GCC only)" "OFF")
option(CXX17           "enable C++17 features" "ON")
option(BIG_ENDIAN_HOST "build for big endian host" "OFF")

IF (CXX17)
    set(CMAKE_CXX_STANDARD 17)
ENDIF()

IF(MSVC)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;PGO" CACHE STRING "" FORCE)
ELSE()
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
ENDIF()

set (COMMON_SOURCES

    ../src/file.h
    ../src/file.cpp
)

set (BDELTA_SOURCES  
    
    ${COMMON_SOURCES}    
    ../src/bdelta.cpp
    ../src/bdelta.h
    ../src/checksum.h
    ../src/libbdelta.cpp
)

set (BPATCH_SOURCES

    ${COMMON_SOURCES} 
    ../src/bpatch.cpp
)

set (BDELTA_STATIC_LIB_SOURCES 
    
    ${COMMON_SOURCES}
    ../src/bdelta.h
    ../src/checksum.h
    ../src/libbdelta.cpp
)

set (BDELTA_SHARED_LIB_SOURCES 
    
    ${BDELTA_STATIC_LIB_SOURCES}
)

IF(MSVC)
    set (BDELTA_SHARED_LIB_SOURCES
    
        ${BDELTA_SHARED_LIB_SOURCES}
        ../src/bdelta.def
    )

    IF(CXX17 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.15)
        message(FATAL_ERROR "Your Visual Studio is too old. Fully updated Visual Studio 2017 or newer is required")
    ENDIF()
    add_compile_options(/Zi   # pdb
                        /W4   # warning level 4
                        /EHsc # exceptions: sync
                        /J    # use unsigned char
                        /Gd   # use cdecl
                        # treat warnings as errors
                        /we4715 # not all control paths return a value
                        /we4828 # disallow invalid characters
                        # prinf-like functions: format mismatch
                        /we4473 # <function> : not enough arguments passed for format string 
                        /we4474 # <function> : too many arguments passed for format string 
                        /we4475 # <function> : length modifier <length> cannot be used with type field character <conversion-specifier> in format specifier 
                        /we4476 # <function> : unknown type field character <conversion-specifier> in format specifier 
                        /we4477 # <function> : format string <format-string> requires an argument of type <type>, but variadic argument <position> has type <type>
                        /we4478 # <function> : positional and non-positional placeholders cannot be mixed in the same format string 
                        /we4775 # nonstandard extension used in format string <format-string> of function <function>
                        /we4776 # %<conversion-specifier> is not allowed in the format string of function <function> 
                        /we4777 # <function> : format string <format-string> requires an argument of type <type>, but variadic argument <position> has type <type>
                        /we4778 # <function> : unterminated format string <format-string>
                        # macro arg mismatch
                        /we4002 # too many actual parameters for macro 'identifier'
                        /we4003 # not enough actual parameters for macro 'identifier'
                        /Zc:threadSafeInit- # https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
                        /MP    # multiprocessor compilation
                        /utf-8 # utf-8 source & exec
                        /GF    # eliminate duplicate strings
                        )
    
    IF (CXX17)
        add_compile_options(/std:c++17)
    ENDIF()
    
    SET(CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
    
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    SET(CMAKE_EXE_LINKER_FLAGS "/LARGEADDRESSAWARE /SUBSYSTEM:CONSOLE")
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF,ICF /DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD -DNDEBUG /Ox /Ob2 /Oi /Ot /Oy /GS- /Gy /GR- /GL /Gw /MT")
    SET(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /GS /MTd -D_DEBUG")
    SET(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    
    set(CMAKE_CXX_FLAGS_PGO ${CMAKE_CXX_FLAGS_RELEASE})
    set(CMAKE_C_FLAGS_PGO ${CMAKE_C_FLAGS_RELEASE})
    
    IF (BDELTA_PGO)
        set(CMAKE_SHARED_LINKER_FLAGS_PGO "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG:PGOptimize")
        set(CMAKE_EXE_LINKER_FLAGS_PGO "${CMAKE_EXE_LINKER_FLAGS_PGO} /LTCG:PGOptimize")
    ELSE()
        set(CMAKE_SHARED_LINKER_FLAGS_PGO "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG:PGInstrument")
        set(CMAKE_EXE_LINKER_FLAGS_PGO "${CMAKE_EXE_LINKER_FLAGS_PGO} /LTCG:PGInstrument")
    ENDIF()
    
ELSE()
    IF(CXX17 AND CMAKE_COMPILER_IS_GNUCC)
        IF(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.2)
            message(FATAL_ERROR "Your GCC is too old. GCC 8.2 or newer is required")
        ENDIF()
    ENDIF()
    add_compile_options(-pipe
                        -W
                        -Wall

                        -Wextra
                        -Wwrite-strings
                        -Wframe-larger-than=16384
                        -Wstack-usage=16384
                        -fdiagnostics-show-option
                        -Wmissing-declarations
                        -Wredundant-decls
                        -Wcast-qual
                        -Wsuggest-attribute=noreturn
                        -Wsuggest-attribute=format
                        -Wunused-but-set-variable
                        -Wunused-but-set-parameter

                        -Wframe-larger-than=4096

                        -Wno-multichar
                        -Wno-strict-aliasing
                        -Wno-missing-field-initializers

                        -Werror=return-type
                        -Werror=pointer-arith
                        -Werror=format
                        -Werror=format-extra-args
                        -Werror=unused-value
                        -Werror=sizeof-pointer-memaccess
                        -Werror=implicit-function-declaration

                        -Werror=missing-declarations
                        -Werror=missing-prototypes
                        -Werror=reorder
                        -Werror=declaration-after-statement
                        -Werror=missing-format-attribute
                        -funsigned-char)
    
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=delete-non-virtual-dtor -U__STRICT_ANSI__ -fno-operator-names")
    IF (CXX17)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    ENDIF()
    SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wimplicit-int -Wmissing-prototypes -Werror=implicit-int")

    SET(CMAKE_CXX_FLAGS_RELEASE  "-O2 -s -fno-rtti -DNDEBUG")
    SET(CMAKE_C_FLAGS_RELEASE    ${CMAKE_CXX_FLAGS_RELEASE})
    SET(CMAKE_CXX_FLAGS_DEBUG    "-O0 -g -D_DEBUG -D__DEBUG__ -DDEBUG_LEVEL=3 -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
    SET(CMAKE_C_FLAGS_DEBUG      ${CMAKE_CXX_FLAGS_DEBUG})
    
    SET(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS} -g")
    SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS} -g")
    
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS} -s")
    SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS} -s")

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    
IF (GCC32)
    add_compile_options (-m32)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
ENDIF()   
ENDIF()

IF (CXX17)
    add_definitions(-DUSE_CXX17)
ENDIF()

add_executable(
    bdelta
    
    ${BDELTA_SOURCES}
)

add_executable(
    bpatch
    
    ${BPATCH_SOURCES}
)

add_library (
    libbdelta-static
    STATIC

    ${BDELTA_STATIC_LIB_SOURCES}
)

add_library (
    libbdelta-shared
    SHARED

    ${BDELTA_SHARED_LIB_SOURCES}
)

SET_TARGET_PROPERTIES(
    libbdelta-static
    PROPERTIES PREFIX      ""
               OUTPUT_NAME libbdelta
               PDB_NAME    libbdelta-static
)

SET_TARGET_PROPERTIES(
    libbdelta-shared
    PROPERTIES PREFIX      ""
               OUTPUT_NAME libbdelta
               PDB_NAME    libbdelta-shared
)

IF (NOT MSVC)
    IF (CXX17)
        SET (
            LIBS

            stdc++fs
        )
    ENDIF()

    target_link_libraries(bdelta ${LIBS})
    target_link_libraries(bpatch ${LIBS})
ENDIF()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT bdelta)
