# Regenerate version_git.h with the current git short hash (+ "-dirty" when the
# firmware/ tree has uncommitted changes). Run both at configure time and on
# every build (via a custom target) so the embedded hash always matches the
# tree that produced the binary.
#
# Inputs (passed with -D): GIT_SRC_DIR, GIT_HDR_IN, GIT_HDR_OUT

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${GIT_SRC_DIR}
    OUTPUT_VARIABLE _hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Scope the dirty check to firmware/ so unrelated repo changes don't flag it.
execute_process(
    COMMAND git status --porcelain -- .
    WORKING_DIRECTORY ${GIT_SRC_DIR}
    OUTPUT_VARIABLE _dirty
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT _hash)
    set(FW_GIT_HASH "unknown")
elseif(_dirty)
    set(FW_GIT_HASH "${_hash}-dirty")
else()
    set(FW_GIT_HASH "${_hash}")
endif()

configure_file(${GIT_HDR_IN} ${GIT_HDR_OUT} @ONLY)
