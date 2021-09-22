cmake_minimum_required(VERSION 3.14)

set(NAME rewind)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# include PUBLIC directories
set(MY_PUBLIC_INCLUDE_DIRECTORIES      include/)

set(MY_PRIVATE_INCLUDE_DIRECTORIES     src/   )

# source files list
file(GLOB_RECURSE MY_SOURCE_FILES "*.c" )
# include files list
file(GLOB_RECURSE MY_INCLUDE_FILES "*.h")

# build the library
add_library(${NAME} SHARED ${MY_INCLUDE_FILES} ${MY_SOURCE_FILES} )

target_include_directories(${NAME} PUBLIC ${MY_PUBLIC_INCLUDE_DIRECTORIES})
target_include_directories(${NAME} PRIVATE ${MY_PRIVATE_INCLUDE_DIRECTORIES})

target_link_libraries(${NAME} PUBLIC pthread)

set_target_properties(${NAME}
    PROPERTIES
    PUBLIC_HEADER "${MY_INCLUDE_FILES}"
)

install (TARGETS ${NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
