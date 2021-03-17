find_package (PkgConfig REQUIRED)

pkg_check_modules (GLIB2 REQUIRED glib-2.0)
pkg_check_modules (GDBUS REQUIRED dbus-glib-1)

find_library(PAM_LIBRARY pam)

include_directories(${GLIB2_INCLUDE_DIRS} ${GDBUS_INCLUDE_DIRS})
include_directories(${CMAKE_BINARY_DIR}/src/)

add_library(pam_kiran_fprintd MODULE pam-kiran-fprintd.c kiran-pam.c)
add_dependencies(pam_kiran_fprintd kiran-biometrics-proxy.h)
set_target_properties(pam_kiran_fprintd PROPERTIES PREFIX "")
target_link_libraries(pam_kiran_fprintd pam_misc ${GLIB2_LIBRARIES} ${GDBUS_LIBRARIES})

install(TARGETS pam_kiran_fprintd LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/security/)
