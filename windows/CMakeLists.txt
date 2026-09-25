cmake_minimum_required(VERSION 3.24)
project(SafeVoidWindows LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "SafeVoid Windows components can only be built on Windows")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build the Credential Provider for x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
add_compile_options(/W4 /permissive- /guard:cf /sdl /EHsc /utf-8)
add_link_options(/guard:cf /DYNAMICBASE /NXCOMPAT)

add_library(safevoid_common STATIC
  common/crypto.cpp
  common/credentials.cpp
  common/hid_device.cpp)
target_include_directories(safevoid_common PUBLIC common ../protocol)
target_link_libraries(safevoid_common PUBLIC bcrypt crypt32 hid setupapi advapi32 shell32 ole32)
set_property(TARGET safevoid_common PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(SafeVoidCredentialProvider SHARED
  credential-provider/class_factory.cpp
  credential-provider/credential.cpp
  credential-provider/dll.cpp
  credential-provider/provider.cpp
  credential-provider/SafeVoidCredentialProvider.def)
target_include_directories(SafeVoidCredentialProvider PRIVATE credential-provider)
target_link_libraries(SafeVoidCredentialProvider PRIVATE safevoid_common credui secur32 shlwapi uuid)
set_property(TARGET SafeVoidCredentialProvider PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Produce one self-contained GUI installer. The provider DLL is built first and
# embedded as RCDATA; SafeVoidSetup extracts and registers it when launched.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_provider_$<CONFIG>.rc"
  CONTENT "101 RCDATA \"$<TARGET_FILE:SafeVoidCredentialProvider>\"\n")
add_executable(SafeVoidSetup WIN32
  setup/main.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/embedded_provider_$<CONFIG>.rc")
add_dependencies(SafeVoidSetup SafeVoidCredentialProvider)
target_link_libraries(SafeVoidSetup PRIVATE safevoid_common netapi32 userenv comctl32 dwmapi uxtheme)
target_link_options(SafeVoidSetup PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
set_property(TARGET SafeVoidSetup PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS SafeVoidSetup SafeVoidCredentialProvider
  RUNTIME DESTINATION . LIBRARY DESTINATION .)
