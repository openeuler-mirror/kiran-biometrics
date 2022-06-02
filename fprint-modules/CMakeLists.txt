find_package (PkgConfig REQUIRED)

pkg_search_module (FINGERPRINT_SDK fingerprint-sdk)

include_directories(${FINGERPRINT_SDK_INCLUDE_DIRS})

if (FINGERPRINT_SDK_FOUND)
    add_library(kiran_arat_fingerprint MODULE aratfingerprint.c)
    target_link_libraries(kiran_arat_fingerprint ${FINGERPRINT_SDK_LIBRARIES})
    install(TARGETS kiran_arat_fingerprint LIBRARY DESTINATION ${MODULE_DIR})

    if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
        add_library(kiran_zk_fingerprint MODULE zkfingerprint.c)
        target_link_libraries(kiran_zk_fingerprint -ldl)
        install(TARGETS kiran_zk_fingerprint LIBRARY DESTINATION ${MODULE_DIR})
    endif()
endif(FINGERPRINT_SDK_FOUND)

install(FILES kiran-fprint-module.h DESTINATION /usr/include/)
