# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake

# The command to remove a file.
RM = /Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/zjz/work/cpp/cpp_test

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/zjz/work/cpp/cpp_test/build

# Include any dependencies generated for this target.
include CMakeFiles/cppTest.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cppTest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cppTest.dir/flags.make

CMakeFiles/cppTest.dir/tests/http_test.cpp.o: CMakeFiles/cppTest.dir/flags.make
CMakeFiles/cppTest.dir/tests/http_test.cpp.o: ../tests/http_test.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/zjz/work/cpp/cpp_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cppTest.dir/tests/http_test.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cppTest.dir/tests/http_test.cpp.o -c /Users/zjz/work/cpp/cpp_test/tests/http_test.cpp

CMakeFiles/cppTest.dir/tests/http_test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cppTest.dir/tests/http_test.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/zjz/work/cpp/cpp_test/tests/http_test.cpp > CMakeFiles/cppTest.dir/tests/http_test.cpp.i

CMakeFiles/cppTest.dir/tests/http_test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cppTest.dir/tests/http_test.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/zjz/work/cpp/cpp_test/tests/http_test.cpp -o CMakeFiles/cppTest.dir/tests/http_test.cpp.s

CMakeFiles/cppTest.dir/tests/map_loader.cpp.o: CMakeFiles/cppTest.dir/flags.make
CMakeFiles/cppTest.dir/tests/map_loader.cpp.o: ../tests/map_loader.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/zjz/work/cpp/cpp_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/cppTest.dir/tests/map_loader.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cppTest.dir/tests/map_loader.cpp.o -c /Users/zjz/work/cpp/cpp_test/tests/map_loader.cpp

CMakeFiles/cppTest.dir/tests/map_loader.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cppTest.dir/tests/map_loader.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/zjz/work/cpp/cpp_test/tests/map_loader.cpp > CMakeFiles/cppTest.dir/tests/map_loader.cpp.i

CMakeFiles/cppTest.dir/tests/map_loader.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cppTest.dir/tests/map_loader.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/zjz/work/cpp/cpp_test/tests/map_loader.cpp -o CMakeFiles/cppTest.dir/tests/map_loader.cpp.s

# Object files for target cppTest
cppTest_OBJECTS = \
"CMakeFiles/cppTest.dir/tests/http_test.cpp.o" \
"CMakeFiles/cppTest.dir/tests/map_loader.cpp.o"

# External object files for target cppTest
cppTest_EXTERNAL_OBJECTS =

cppTest: CMakeFiles/cppTest.dir/tests/http_test.cpp.o
cppTest: CMakeFiles/cppTest.dir/tests/map_loader.cpp.o
cppTest: CMakeFiles/cppTest.dir/build.make
cppTest: /usr/local/lib/libboost_system-mt.dylib
cppTest: /usr/local/lib/libboost_filesystem-mt.dylib
cppTest: /usr/local/lib/libboost_iostreams-mt.dylib
cppTest: /usr/local/lib/libboost_regex-mt.dylib
cppTest: /usr/local/lib/libSDLmain.a
cppTest: /usr/local/lib/libSDL.dylib
cppTest: /usr/local/lib/libSDL_image.dylib
cppTest: /usr/local/lib/libyaml-cpp.0.6.2.dylib
cppTest: CMakeFiles/cppTest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/zjz/work/cpp/cpp_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable cppTest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cppTest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cppTest.dir/build: cppTest

.PHONY : CMakeFiles/cppTest.dir/build

CMakeFiles/cppTest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cppTest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cppTest.dir/clean

CMakeFiles/cppTest.dir/depend:
	cd /Users/zjz/work/cpp/cpp_test/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/zjz/work/cpp/cpp_test /Users/zjz/work/cpp/cpp_test /Users/zjz/work/cpp/cpp_test/build /Users/zjz/work/cpp/cpp_test/build /Users/zjz/work/cpp/cpp_test/build/CMakeFiles/cppTest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cppTest.dir/depend

