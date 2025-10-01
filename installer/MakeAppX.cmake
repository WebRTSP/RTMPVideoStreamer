#"C:\Program Files (x86)\Windows Kits\10\App Certification Kit\makeappx.exe" pack /d ./install /p RTMPVideoStreamer.appx /o

find_program(
    MAKEAPPX MakeAppx.exe
    HINTS "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/App Certification Kit"
    REQUIRED
)

set(APPX_TARGET_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.appx")

execute_process(
    COMMAND "${MAKEAPPX}" pack /d "${CPACK_TEMPORARY_DIRECTORY}" /p "${APPX_TARGET_PATH}" /o
)

set(CPACK_EXTERNAL_BUILT_PACKAGES "${APPX_TARGET_PATH}")
