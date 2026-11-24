# Installing the terminal (cmake --install) and packaging it (cpack) as .deb or .rpm.
# The packages take Qt from the system: built on the oldest release we support, they install on
# it and every newer one (no private Qt API, see ui/CMakeLists.txt).
include(GNUInstallDirs)

set(WMS_APP_ID io.github.isnazarii.BerryWms)

# The build keeps the name wms_terminal (docs and tools use it); installed, it is a command.
# install(PROGRAMS) does not rewrite RPATH, which is fine: system Qt adds none.
install(PROGRAMS $<TARGET_FILE:wms_terminal> DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME berry-wms)
install(FILES ${WMS_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${WMS_APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
# udev reads only /usr/lib/udev/rules.d (also on 64-bit Fedora, so not LIBDIR). Off for
# an install into the home folder, where the rule would do nothing.
option(WMS_INSTALL_UDEV_RULE "Install the udev rule that gives the scale port to the logged-in user" ON)
# The license and the notices of the third-party parts must travel with every copy.
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE.md ${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/berry-wms)
if(WMS_INSTALL_UDEV_RULE)
    install(FILES 70-berry-wms-scale.rules DESTINATION lib/udev/rules.d)
endif()

set(CPACK_PACKAGE_NAME berry-wms)
set(CPACK_PACKAGE_VENDOR "Berry WMS")
set(CPACK_PACKAGE_CONTACT "IsNazarii <IsNazarii@users.noreply.github.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/IsNazarii/Berry-WMS-Terminal")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Berry receiving terminal: weighing, trucks, supplier payments")
set(CPACK_PACKAGE_DESCRIPTION
    "Offline-first terminal of a berry receiving point: receiving and weighing pallets on \
industrial scales, loading trucks and the warehouse, printing receipts and paying suppliers.")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.md)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)
# Folders as every package has them, whatever the umask of the machine that builds.
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Debian/Ubuntu: Qt libraries are found by dpkg-shlibdeps; QML modules and image plugins
# are loaded at run time, so nothing links them and they are listed by hand.
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-templates, \
qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-window, \
qml6-module-qtqml-workerscript, qml6-module-qtcore, qt6-svg-plugins")
# Ukrainian texts of Qt's own dialogs; native Wayland instead of XWayland.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-translations-l10n, qt6-wayland")

# Fedora: rpmbuild finds the Qt libraries; qtdeclarative holds every QML module we import.
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
# SPDX name; commercial use needs a separate license from the author (README).
set(CPACK_RPM_PACKAGE_LICENSE "PolyForm-Noncommercial-1.0.0")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative >= 6.8, qt6-qtsvg")
set(CPACK_RPM_PACKAGE_SUGGESTS "qt6-qttranslations, qt6-qtwayland")
# These folders belong to the system's own packages; owning them too makes rpm complain.
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications
    /usr/share/doc
    /usr/share/icons
    /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps
    /usr/lib/udev
    /usr/lib/udev/rules.d)

include(CPack)
