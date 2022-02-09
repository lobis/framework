macro(ENABLE_TESTING_CONDITIONALLY)
    if (NOT DEFINED TEST OR NOT TEST)
        set(TEST OFF)
        message(STATUS "Testing disabled (Disabled by default, enabled via -DTEST=ON flag)")
    endif ()
    if (TEST)
        message(STATUS "Testing enabled (Disabled by default, enabled via -DTEST=ON flag)")
        enable_testing()
    endif ()
endmacro()

macro(ADD_TEST)
    if (TEST)
        message(STATUS "Adding tests at ${CMAKE_CURRENT_SOURCE_DIR}")
        add_subdirectory(test)
    endif ()
endmacro()
