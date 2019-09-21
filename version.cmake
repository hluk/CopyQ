set(copyq_version "v3.9.2")

find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND
        "${GIT_EXECUTABLE}" describe --tags
        RESULT_VARIABLE copyq_git_describe_result
        OUTPUT_VARIABLE copyq_git_describe_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(copyq_git_describe_result EQUAL 0)
        set(copyq_version "${copyq_git_describe_output}")
    endif()
endif()

message(STATUS "Building CopyQ version ${copyq_version}.")

configure_file("${INPUT_FILE}" "${OUTPUT_FILE}")
