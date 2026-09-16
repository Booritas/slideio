# Fails the build if TARGET_FILE exports symbols belonging to a bundled
# third-party library.
#
# This project links two different builds of libtiff and two different libjpeg
# implementations into one process: the ndpi driver carries the ndpi-libtiff and
# ndpi-libjpeg-turbo forks, everything else carries libtiff and libjpeg. The
# symbol names are identical in both. While each shared library keeps its copy
# private the arrangement is fine, but the moment those names are exported, which
# copy answers a given call becomes a function of load order rather than of what
# the caller was compiled against -- and the structs differ in size, so the
# mismatch surfaces as libjpeg's "JPEG parameter struct mismatch" at runtime
# instead of as a link error.
#
# Invoked as a POST_BUILD step by HIDE_THIRD_PARTY_SYMBOLS. Windows is not
# checked: there, only __declspec(dllexport) symbols are exported at all.

if(NOT DEFINED TARGET_FILE)
    message(FATAL_ERROR "assert_no_thirdparty_exports: TARGET_FILE is not set")
endif()

find_program(NM_TOOL NAMES nm)
if(NOT NM_TOOL)
    # A missing nm is not a reason to fail someone's build; the check is a guard
    # against regressions, not a correctness requirement of the output.
    message(STATUS "assert_no_thirdparty_exports: nm not found, skipping ${TARGET_FILE}")
    return()
endif()

# APPLE: -U means "defined only". ELF: release builds are stripped (-s), so ask
# the dynamic symbol table, which is what the loader would actually use anyway.
if(APPLE_HOST)
    set(NM_ARGS -g -U)
else()
    set(NM_ARGS -D --defined-only)
endif()

execute_process(
    COMMAND ${NM_TOOL} ${NM_ARGS} ${TARGET_FILE}
    OUTPUT_VARIABLE symbol_table
    ERROR_QUIET
    RESULT_VARIABLE nm_result)

if(NOT nm_result EQUAL 0)
    message(STATUS "assert_no_thirdparty_exports: nm failed on ${TARGET_FILE}, skipping")
    return()
endif()

# A defined, exported text symbol named jpeg_* or TIFF*. The leading underscore
# is Mach-O's; ELF has none.
string(REGEX MATCHALL "[ \t]T _?(jpeg_|TIFF)[A-Za-z0-9_]*" leaked "${symbol_table}")

if(leaked)
    list(LENGTH leaked leaked_count)
    list(SUBLIST leaked 0 5 sample)
    string(REPLACE ";" "\n    " sample_text "${sample}")
    get_filename_component(target_name ${TARGET_FILE} NAME)
    message(FATAL_ERROR
        "${target_name} exports ${leaked_count} bundled third-party symbol(s), e.g.:\n"
        "    ${sample_text}\n"
        "Exported libtiff/libjpeg symbols let one driver's copy satisfy another "
        "driver's calls by load order. Route this target through "
        "HIDE_THIRD_PARTY_SYMBOLS(), and see cmake-scripts/assert_no_thirdparty_exports.cmake.")
endif()
