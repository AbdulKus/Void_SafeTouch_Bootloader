cmake_minimum_required(VERSION 3.24)
project(SafeTouchWindows LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "SafeTouch Windows components can only be built on Windows")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build the Credential Provider for x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
add_compile_options(/W4 /permissive- /guard:cf /sdl /EHsc /utf-8)
add_link_options(/guard:cf /DYNAMICBASE /NXCOMPAT)

add_library(safetouch_common STATIC
  common/crypto.cpp
  common/credentials.cpp
  common/hid_device.cpp)
target_include_directories(safetouch_common PUBLIC common ../protocol)
target_link_libraries(safetouch_common PUBLIC bcrypt crypt32 hid setupapi advapi32 shell32 ole32)
set_property(TARGET safetouch_common PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(SafeTouchSetup setup/main.cpp)
target_link_libraries(SafeTouchSetup PRIVATE safetouch_common netapi32 userenv)
set_property(TARGET SafeTouchSetup PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(SafeTouchCredentialProvider SHARED
  credential-provider/class_factory.cpp
  credential-provider/credential.cpp
  credential-provider/dll.cpp
  credential-provider/provider.cpp
  credential-provider/SafeTouchCredentialProvider.def)
target_include_directories(SafeTouchCredentialProvider PRIVATE credential-provider)
target_link_libraries(SafeTouchCredentialProvider PRIVATE safetouch_common credui secur32 shlwapi uuid)
set_property(TARGET SafeTouchCredentialProvider PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS SafeTouchSetup SafeTouchCredentialProvider
  RUNTIME DESTINATION . LIBRARY DESTINATION .)
