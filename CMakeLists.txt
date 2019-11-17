cmake_minimum_required(VERSION 3.15)
project(qt_multithreading)

set(CMAKE_CXX_STANDARD 20) # we are from the future

# compiler optimization
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# needed by qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# if e.g. Qt 5.1 and 4.1.2 are available and we need at least 4.1.1, use 4.1.2, not 5.1
set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)

# find qt
find_package(
        Qt5 5.13.2 COMPONENTS Core REQUIRED
)

# ~~~ example one ~~~

add_executable(
        ${PROJECT_NAME}_example_one
        src/examples/one/main.cpp # the framework is header only, unless you are using the Q_OBJECT macro, only this is needed
        src/examples/one/ConcreteProcessor.h # has to be added here, since a Q_OBJECT macro is present in the file
        src/examples/one/Communicator.h # has to be added here, since a Q_OBJECT macro is present in the file
)
# link qt libraries
target_link_libraries(
        ${PROJECT_NAME}_example_one Qt5::Core
)

# ~~~ example two ~~~

add_executable(
        ${PROJECT_NAME}_example_two
        src/examples/two/main.cpp # the framework is header only, unless you are using the Q_OBJECT macro, only this is needed
)
# link qt libraries
target_link_libraries(
        ${PROJECT_NAME}_example_two Qt5::Core
)

# ~~~ example three ~~~

add_executable(
        ${PROJECT_NAME}_example_three
        src/examples/three/main.cpp # the framework is header only, unless you are using the Q_OBJECT macro, only this is needed
)
# link qt libraries
target_link_libraries(
        ${PROJECT_NAME}_example_three Qt5::Core
)
