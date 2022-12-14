########## LFAST Controller Utilities Unit Tests##############

# set our include directories to look for header files

include_directories( ${CMAKE_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

# cmake -DBUILD_TESTS=true -GNinja ..

#=================================================================================================#
#========================================= gtest  ================================================#
#=================================================================================================#
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

#=================================================================================================#
#========================================= project test executables ==============================#
#=================================================================================================#

add_executable(
  math_util_tests
  math_util_tests.cc
)
target_link_libraries(
  math_util_tests
  GTest::gtest_main
)

#### Bringing it all together
include(GoogleTest)
gtest_discover_tests(math_util_tests)

