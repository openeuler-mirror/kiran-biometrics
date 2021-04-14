project(kiran-biometrics)
cmake_minimum_required(VERSION 3.5)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(DEBUG 1)
else()
  set(DEBUG 0)
endif()

include(GNUInstallDirs)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(VERSION 0.1)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(MODULE_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/kiran-fprint-modules/)
set(SERVICE_NAME "com.kylinsec.Kiran.SystemDaemon.Biometrics")
set(SERVICE_PATH "/com/kylinsec/Kiran/SystemDaemon/Biometrics")
set(SERVICE_INTERFACE "com.kylinsec.Kiran.SystemDaemon.Biometrics")
set(INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/libexec)

if ("${ENABLE_FACE}" STREQUAL "yes")
    set(CMAKE_C_FLAGS "{CMAKE_C_FLAGS}" -DHAVE_KIRAN_FACE)
endif()

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(fprint-modules)
add_subdirectory(pam)
add_subdirectory(po)
