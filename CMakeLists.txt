cmake_minimum_required(VERSION 3.28)
project(evo_image_filter_cpp)

set(CMAKE_CXX_STANDARD 23)
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS gil)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(args REQUIRED)

include_directories(${Boost_INCLUDE_DIRS} ${args_INCLUDE_DIRS})
include_directories(include)

add_executable(${PROJECT_NAME}
        include/Timestamper.h
        include/Parallelizer.h
        include/StepSorter.h
        include/Shaper.h
        include/Util.h
        include/ImageOps.h
        include/Types.h
        src/Shaper.cpp
        src/Util.cpp
        src/ImageOps.cpp
        src/Main.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC stdc++fs)
target_link_libraries(${PROJECT_NAME} PRIVATE
        JPEG::JPEG
        PNG::PNG
)
