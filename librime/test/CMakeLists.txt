if(NOT (MSVC AND BUILD_SHARED_LIBS))

aux_source_directory(. RIME_TEST_SRC)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/test)
add_executable(rime_test ${RIME_TEST_SRC})
target_link_libraries(rime_test
                      ${RIME_LIBRARY} ${RIME_GEARS_LIBRARY}
                      ${GTEST_LIBRARIES})
add_dependencies(rime_test ${RIME_LIBRARY} ${RIME_GEARS_LIBRARY})

file(COPY ${PROJECT_SOURCE_DIR}/data/test/config_test.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/test/dictionary_test.dict.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})

set(RIME_TEST_EXECUTABLE ${EXECUTABLE_OUTPUT_PATH}/rime_test${EXT})
add_test(rime_test ${RIME_TEST_EXECUTABLE})

endif(NOT (MSVC AND BUILD_SHARED_LIBS))
