# Keep the pinned upstream source clean. GCC 16 no longer supplies uint32_t
# transitively to glslang's SpvBuilder.h (verified stage3-tools-debug.log).
function(hazel_shaderc_header_compatibility)
    target_compile_options(SPIRV PRIVATE
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-include>"
        "$<$<CXX_COMPILER_ID:GNU,Clang>:cstdint>")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL hazel_shaderc_header_compatibility)
