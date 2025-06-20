include(FetchContent)
find_package(FMT REQUIRED)
set (RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set (RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set (RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF CACHE BOOL "" FORCE)
set (RAPIDJSON_ENABLE_INSTRUMENTATION_OPT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    rapidjson
    #    GIT_TAG f9d53419e912910fd8fa57d5705fa41425428c35 - latest but broken revision
    URL https://github.com/Tencent/rapidjson/archive/973dc9c06dcd3d035ebd039cfb9ea457721ec213.tar.gz
    URL_HASH SHA256=d0c9e52823d493206eb721d38cb3a669ca0212360862bd15a3c2f7d35ea7c6f7
)
FetchContent_MakeAvailable(rapidjson)

find_package(RapidJSON REQUIRED
             PATHS "${rapidjson_BINARY_DIR}"
             NO_CMAKE_FIND_ROOT_PATH
             NO_DEFAULT_PATH)

add_library(RapidJson INTERFACE)
target_include_directories(RapidJson
    INTERFACE
        $<BUILD_INTERFACE:${RapidJSON_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

if (JINJA2CPP_BUILD_TESTS)
    set (JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set (JSON_Install OFF CACHE BOOL "" FORCE)
    set (JSON_MultipleHeaders ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/8c391e04fe4195d8be862c97f38cfe10e2a3472e.tar.gz
        URL_HASH SHA256=8ca375182e9557612f043eaa62dfc4224b41ddf07af704577666aadb7dd99a79
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

install (FILES
        thirdparty/nonstd/expected-lite/include/nonstd/expected.hpp
        thirdparty/nonstd/variant-lite/include/nonstd/variant.hpp
        thirdparty/nonstd/optional-lite/include/nonstd/optional.hpp
        thirdparty/nonstd/string-view-lite/include/nonstd/string_view.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nonstd)

install (TARGETS RapidJson
    EXPORT InstallTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
)
