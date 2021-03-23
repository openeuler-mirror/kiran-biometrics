find_package (PkgConfig REQUIRED)

pkg_check_modules (GLIB2 REQUIRED glib-2.0)
pkg_check_modules (GDBUS REQUIRED dbus-glib-1)
pkg_check_modules (GIO REQUIRED gio-2.0)
pkg_check_modules (GMODULE REQUIRED gmodule-2.0)
pkg_check_modules (OPENCV_GLIB REQUIRED opencv-glib)
pkg_check_modules (ZMQ REQUIRED libzmq)
pkg_check_modules (GLIB_JSON REQUIRED json-glib-1.0)
pkg_check_modules (ZLOG REQUIRED zlog)

configure_file(${SRC_DIR}/config.h.in config.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${GLIB2_INCLUDE_DIRS} ${GDBUS_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${OPENCV_GLIB_INCLUDE_DIRS} ${ZMQ_INCLUDE_DIRS} ${GLIB_JSON_INCLUDE_DIRS} ${ZLOG_INCLUDE_DIRS})
add_executable (kiran_biometrics_manager main.c kiran-biometrics.c kiran-fprint-module.c kiran-fprint-manager.c kiran-face-manager.c)
add_dependencies(kiran_biometrics_manager kiran-biometrics-stub.h)
target_link_libraries(kiran_biometrics_manager ${GLIB2_LIBRARIES} ${GDBUS_LIBRARIES} ${GIO_LIBRARIES} ${GMODULE_LIBRARIES} ${OPENCV_GLIB_LIBRARIES} ${ZMQ_LIBRARIES} ${GLIB_JSON_LIBRARIES} ${ZLOG_LIBRARIES})
install(TARGETS kiran_biometrics_manager RUNTIME DESTINATION ${INSTALL_BINDIR})
