find_package(Qt6 REQUIRED COMPONENTS Test Sql)

enable_testing()

add_executable(test_password_utils
    test_password_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/password_utils.h
)
target_include_directories(test_password_utils PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_password_utils PRIVATE Qt6::Test Qt6::Core)

add_test(NAME test_password_utils COMMAND test_password_utils)

add_executable(test_validator
    test_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/validator.h
    ${CMAKE_SOURCE_DIR}/src/utils/validator.cpp
)
target_include_directories(test_validator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_validator PRIVATE Qt6::Test Qt6::Core Qt6::Sql)
