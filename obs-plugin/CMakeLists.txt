cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES CXX)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

target_sources(
  ${CMAKE_PROJECT_NAME}
	PRIVATE
	resources/privacy-screens.qrc
	src/display-settings.cpp
	src/display-settings.hpp
	src/named-pipe-server.cpp
	src/named-pipe-server.hpp
	src/plugin-main.cpp
	src/privacy-state.cpp
	src/privacy-state.hpp
	src/protection-display.cpp
	src/protection-display.hpp
	src/protocol.cpp
	src/protocol.hpp
)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
	OBS::libobs
	OBS::obs-frontend-api
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /permissive-)
endif()

set_target_properties_plugin(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES OUTPUT_NAME ${_name}
)

include(CTest)

if(BUILD_TESTING)
  add_executable(privacy-guard-native-tests)
  target_sources(
	privacy-guard-native-tests
	PRIVATE
	  tests/native-tests.cpp
	  src/privacy-state.cpp
	  src/privacy-state.hpp
	  src/protocol.cpp
	  src/protocol.hpp
  )
  target_include_directories(
	privacy-guard-native-tests
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  target_compile_features(privacy-guard-native-tests PRIVATE cxx_std_20)
  if(MSVC)
	target_compile_options(
	  privacy-guard-native-tests
	  PRIVATE /W4 /permissive-
	)
  endif()
  add_test(NAME privacy-guard-native-tests COMMAND privacy-guard-native-tests)
endif()
