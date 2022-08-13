#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Osmium Surplus - test
#
#-----------------------------------------------------------------------------

add_test(NAME osp-analyze-line-or-polygon
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/osp-analyze-line-or-polygon.sh ${CMAKE_SOURCE_DIR})

#-----------------------------------------------------------------------------
