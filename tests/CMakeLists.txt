# One executable per tst_*.cpp, each its own CTest case, laid out to mirror the
# headers they cover: tests/core/tst_config.cpp covers src/core/config.hpp.
#
# Everything under src/ is header-only, so a test compiles the headers it covers
# and nothing else. A standalone project rather than a subdirectory: the app
# builds with the Makefile, and this must not suggest otherwise.
#
# AUTOMOC is what QTest needs: every test declares a Q_OBJECT class and includes
# the moc output at the end of its own file.

cmake_minimum_required(VERSION 3.22)
project(mouse-battery-monitor-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

enable_testing()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

set(CORE_TESTS battery_monitor config device_manager hotplug_scheduler)
set(DEVICES_TESTS endgame_gear_device vaxee_device)

function(add_unit_test directory name)
    add_executable(tst_${name} "${directory}/tst_${name}.cpp")
    target_include_directories(tst_${name} PRIVATE "${SRC}" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(tst_${name} PRIVATE Qt6::Test)
    target_compile_options(tst_${name} PRIVATE -Wall -Wextra -Wsuggest-override
                                               -Wnon-virtual-dtor -Wshadow)
    # Named for where it sits, so `ctest -R core` runs one layer of them.
    add_test(NAME ${directory}/${name} COMMAND tst_${name})
endfunction()

foreach(test IN LISTS CORE_TESTS)
    add_unit_test(core ${test})
endforeach()

foreach(test IN LISTS DEVICES_TESTS)
    add_unit_test(devices ${test})
endforeach()

# battery_monitor.hpp updates the tray icon, a QSystemTrayIcon.
target_link_libraries(tst_battery_monitor PRIVATE Qt6::Widgets)
target_link_libraries(tst_hotplug_scheduler PRIVATE Qt6::Widgets)
