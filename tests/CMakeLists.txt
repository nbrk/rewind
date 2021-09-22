link_libraries(rewind)

enable_testing()
link_libraries(check rt m subunit)

add_executable(tst_history tst_history.c)
add_test(NAME history COMMAND tst_history)


