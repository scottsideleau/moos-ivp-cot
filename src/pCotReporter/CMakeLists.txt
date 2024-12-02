#--------------------------------------------------------
# The CMakeLists.txt for:               pCotReporter
# Author(s):                            Scott R. Sideleau
#--------------------------------------------------------

# Set System Specific Libraries
if (${WIN32})
  # Windows Libraries
  SET(SYSTEM_LIBS
     wsock32 )
else (${WIN32})
  # Linux and Apple Libraries
  SET(SYSTEM_LIBS
      m
      pthread )
endif (${WIN32})

FILE(GLOB SRC *.cpp)

# Add Local Libraries to Includes
INCLUDE_DIRECTORIES( 
  ${CMAKE_SOURCE_DIR}/lib/rapidxml
  ${CMAKE_SOURCE_DIR}/lib/asio 
)

ADD_EXECUTABLE(pCotReporter ${SRC})

TARGET_LINK_LIBRARIES(pCotReporter
   ${MOOS_LIBRARIES}
   ${SYSTEM_LIBS})
