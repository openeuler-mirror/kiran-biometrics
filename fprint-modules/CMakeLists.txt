find_package (PkgConfig REQUIRED)

pkg_search_module (FINGERPRINT_SDK fingerprint-sdk)

if (FINGERPRINT_SDK_FOUND)
    link_directories(kiran_arat_fingerprint PRIVATE ${FINGERPRINT_SDK_LIBRARY_DIRS})
    add_library(kiran_arat_fingerprint MODULE aratfingerprint.c)
    target_link_libraries(kiran_arat_fingerprint ${FINGERPRINT_SDK_LIBRARIES})
    install(TARGETS kiran_arat_fingerprint LIBRARY DESTINATION ${MODULE_DIR})

#    add_library(kiran_zk_fingerprint MODULE zkfingerprint.c)
#    target_link_libraries(kiran_zk_fingerprint ${FINGERPRINT_SDK_LIBRARIES})
#    link_directories(kiran_zk_fingerprint  PRIVATE ${FINGERPRINT_SDK_LIBRARY_DIRS})
#    install(TARGETS kiran_zk_fingerprint LIBRARY DESTINATION ${MODULE_DIR})
endif(FINGERPRINT_SDK_FOUND)

install(FILES kiran-fprint-module.h DESTINATION /usr/include/)
