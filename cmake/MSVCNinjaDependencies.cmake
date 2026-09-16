# Ninja matches /showIncludes output byte-for-byte. CMake's AUTO decoding can
# corrupt localized MSVC output when the compiler and console encodings differ.
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
   AND CMAKE_GENERATOR MATCHES "Ninja")
    set(_showincludes_dir "${CMAKE_BINARY_DIR}/CMakeFiles/ShowIncludesRaw")
    file(MAKE_DIRECTORY "${_showincludes_dir}")
    file(WRITE "${_showincludes_dir}/probe.h" "\n")
    file(WRITE "${_showincludes_dir}/probe.cpp" "#include \"probe.h\"\n")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /EP probe.cpp
        WORKING_DIRECTORY "${_showincludes_dir}"
        OUTPUT_VARIABLE _showincludes_output
        ERROR_VARIABLE _showincludes_error
        RESULT_VARIABLE _showincludes_result
        ENCODING NONE
    )
    string(APPEND _showincludes_output "${_showincludes_error}")
    if(_showincludes_result EQUAL 0 AND
       "${_showincludes_output}" MATCHES "(^|\n)([^:\n][^:\n]+:[^:\n]*[^: \n][^: \n]:?[ \t]+)([A-Za-z]:\\\\|\\./|\\.\\\\|/)")
        set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    else()
        message(FATAL_ERROR "Could not detect MSVC header dependency prefix: ${_showincludes_output}${_showincludes_error}")
    endif()
    unset(_showincludes_dir)
    unset(_showincludes_output)
    unset(_showincludes_error)
    unset(_showincludes_result)
endif()
