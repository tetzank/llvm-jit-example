cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED CMAKE_CXX_FLAGS)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -fno-rtti -fopenmp" CACHE STRING "Flags used by the compiler during all build types.")
endif()
# build type defaults to release
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS) Debug Release RelWithdebInfo MinSizeRel.")
endif()

project(llvmjitexample CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find llvm, quick&dirty
find_path(LLVM_INCLUDE_DIR
	llvm/IR/IRBuilder.h
	HINTS external/llvm/llvm/install ENV LLVM_ROOT
	PATH_SUFFIXES include
)
find_library(LLVM_LIBRARIES
	LLVM
	HINTS external/llvm/llvm/install ENV LLVM_ROOT
	PATH_SUFFIXES lib build/lib
)
include_directories(${LLVM_INCLUDE_DIR})


add_executable(sum sum.cpp)
target_link_libraries(sum ${LLVM_LIBRARIES})

add_executable(sumDebug sumDebug.cpp)
target_link_libraries(sumDebug ${LLVM_LIBRARIES})
