set(robotname iCubGenova02)

file(GLOB ini ${CMAKE_CURRENT_SOURCE_DIR}/*.ini)
file(GLOB urdf ${CMAKE_CURRENT_SOURCE_DIR}/*.urdf)

yarp_install(FILES ${ini} DESTINATION ${CODYCO_ROBOTS_INSTALL_DIR}/${robotname})
yarp_install(FILES ${urdf} DESTINATION ${CODYCO_ROBOTS_INSTALL_DIR}/${robotname})
