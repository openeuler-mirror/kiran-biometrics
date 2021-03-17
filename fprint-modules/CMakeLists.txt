find_package (PkgConfig REQUIRED)

pkg_check_modules (FINGERPRINT_SDK REQUIRED fingerprint-sdk)

link_directories(kiran_arat_fingerprint PRIVATE ${FINGERPRINT_SDK_LIBRARY_DIRS})
link_directories(kiran_zk_fingerprint  PRIVATE ${FINGERPRINT_SDK_LIBRARY_DIRS})

add_library(kiran_zk_fingerprint MODULE zkfingerprint.c)
add_library(kiran_arat_fingerprint MODULE aratfingerprint.c)
target_link_libraries(kiran_zk_fingerprint ${FINGERPRINT_SDK_LIBRARIES})
target_link_libraries(kiran_arat_fingerprint ${FINGERPRINT_SDK_LIBRARIES})

message ("-->${FINGERPRINT_SDK_LIBRARY_DIRS}")


install(TARGETS kiran_zk_fingerprint LIBRARY DESTINATION ${MODULE_DIR})
install(TARGETS kiran_arat_fingerprint LIBRARY DESTINATION ${MODULE_DIR})
