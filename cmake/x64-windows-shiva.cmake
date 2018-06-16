set(VCPKG_TARGET_ARCHITECTURE x64)

set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if (CMAKE_HOST_WIN32)
    if (PORT MATCHES "sfml-")
        set(VCPKG_LIBRARY_LINKAGE static)
    endif ()
endif ()

