#--------------------------------------------------------
# The CMakeLists.txt for:                       pGenRescue
#--------------------------------------------------------

SET(SRC
  GenRescue.cpp
  main.cpp
)

ADD_EXECUTABLE(pGenRescue ${SRC})

TARGET_LINK_LIBRARIES(pGenRescue
   ${MOOS_LIBRARIES}
   apputil
   geometry
   mbutil
   m
   pthread)
