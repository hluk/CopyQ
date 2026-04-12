set(copyq_version "15.0.0")

set(copyq_git_describe_result 1)

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
        # Strip leading "v" from version tag.
        string(REGEX REPLACE "^v" "" copyq_git_describe_output "${copyq_git_describe_output}")
        # Make commit-count suffix sortable:
        #   v14.0.0-4-g1175475d  ->  14.0.0.4-g1175475d
        string(REGEX REPLACE "-([0-9]+)-g([0-9a-f]+)$" ".\\1-g\\2"
            copyq_git_describe_output "${copyq_git_describe_output}")
        set(copyq_version "${copyq_git_describe_output}")
    endif()
endif()

if(NOT GIT_FOUND OR NOT copyq_git_describe_result EQUAL 0)
    # Fallback for CI without full git history (e.g., shallow clone).
    # Extract clean version from tag ref if available.
    set(copyq_github_ref "$ENV{GITHUB_REF}")
    if(copyq_github_ref MATCHES "^refs/tags/v(.+)$")
        set(copyq_version "${CMAKE_MATCH_1}")
    endif()
endif()
