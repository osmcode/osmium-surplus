#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Osmium Surplus - src
#
#-----------------------------------------------------------------------------

add_custom_target(clang-tidy-all)

function(exec _name)
    cmake_parse_arguments(PARSE_ARGV 1 "EXEC" "WITH_GDAL;WITH_SQLITE" "" "SRCS")
    message(STATUS "  Executable ${_name} WITH_GDAL=${EXEC_WITH_GDAL} WITH_SQLITE=${EXEC_WITH_SQLITE} sources=${EXEC_SRCS}")
    set(_libs ${OSMIUM_IO_LIBRARIES})
    set(_srcs ${EXEC_SRCS})
    if(${EXEC_WITH_GDAL})
        LIST(APPEND _libs ${GDAL_LIBRARIES})
    endif()
    if(${EXEC_WITH_SQLITE})
        LIST(APPEND _libs ${SQLite3_LIBRARIES})
        LIST(APPEND _srcs db.cpp)
    endif()
    add_executable(${_name} ${_name}.cpp ${_srcs})
    target_link_libraries(${_name} ${_libs})

    install(TARGETS ${_name} DESTINATION bin)

    add_custom_command(OUTPUT ${_name}.clang-tidy.log
        COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} ${_name}.cpp ${_srcs} > ${CMAKE_BINARY_DIR}/src/${_name}.clang-tidy.log 2>&1
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        DEPENDS ${_name}.cpp ${_srcs}
    )
    add_custom_target(clang-tidy-${_name}
        DEPENDS ${_name}.clang-tidy.log
    )
    add_dependencies(clang-tidy-all clang-tidy-${_name})
endfunction()

#-----------------------------------------------------------------------------

exec(osp-analyze-limits)
exec(osp-analyze-line-or-polygon SRCS filter.cpp)
exec(osp-analyze-relation-types WITH_SQLITE SRCS app.cpp)
exec(osp-changeset-check-timestamps SRCS app.cpp)
exec(osp-check-characters)
exec(osp-filter-relations-and-members SRCS app.cpp)
exec(osp-find-and-fix-control-characters SRCS app.cpp)
exec(osp-find-colocated-nodes WITH_GDAL WITH_SQLITE)
exec(osp-find-multipolygon-problems WITH_GDAL WITH_SQLITE)
exec(osp-find-orphans WITH_GDAL WITH_SQLITE)
exec(osp-find-relation-problems WITH_GDAL WITH_SQLITE)
exec(osp-find-unusual-tags WITH_SQLITE)
exec(osp-find-way-problems WITH_GDAL WITH_SQLITE)
exec(osp-history-stats-basic WITH_SQLITE SRCS app.cpp)
exec(osp-history-stats-users WITH_SQLITE SRCS app.cpp)
exec(osp-mark-topo-nodes)
exec(osp-proc-remove-tags SRCS filter.cpp)
exec(osp-stats-basic WITH_SQLITE SRCS app.cpp)
exec(osp-stats-duplicate-segments)
exec(osp-stats-tags)
exec(osp-stats-tags-on-nodes SRCS app.cpp)
exec(osp-stats-way-node-refs-delta SRCS app.cpp)
exec(osp-stats-way-nodes)
exec(osp-stats-way-nodes-idx)

#-----------------------------------------------------------------------------
