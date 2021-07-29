function(dbusxml2c _interface _output _mode _prefix)
  add_custom_target(${_output}
    COMMAND
    dbus-binding-tool --mode=${_mode} --prefix=${_prefix}
    --output=${CMAKE_BINARY_DIR}/src/${_output} ${_interface}
    DEPENDS ${INTERFACE})
endfunction()

dbusxml2c(${CMAKE_CURRENT_SOURCE_DIR}/com.kylinsec.Kiran.SystemDaemon.Biometrics.xml kiran-biometrics-stub.h glib-server SystemDaemon)
dbusxml2c(${CMAKE_CURRENT_SOURCE_DIR}/com.kylinsec.Kiran.SystemDaemon.Biometrics.xml kiran-biometrics-proxy.h glib-client SystemDaemon)

configure_file(com.kylinsec.Kiran.SystemDaemon.Biometrics.service.in ${PROJECT_BINARY_DIR}/data/com.kylinsec.Kiran.SystemDaemon.Biometrics.service)
configure_file(kiran-system-daemon-biometrics.service.in ${PROJECT_BINARY_DIR}/data/kiran-system-daemon-biometrics.service)
configure_file(settings.conf.in settings.conf)

install(FILES kiran_biometrics.conf DESTINATION /etc/dbus-1/system.d)
install(FILES ${PROJECT_BINARY_DIR}/data/com.kylinsec.Kiran.SystemDaemon.Biometrics.service DESTINATION /usr/share/dbus-1/system-services)
install(FILES ${PROJECT_BINARY_DIR}/data/kiran-system-daemon-biometrics.service DESTINATION /usr/lib/systemd/system/)
install(FILES ${PROJECT_BINARY_DIR}/data/settings.conf DESTINATION ${PROJECT_NAME})
