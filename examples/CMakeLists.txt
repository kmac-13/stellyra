cmake_minimum_required( VERSION 3.16 )

project( StellyraExamples LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Collect Stellyra files
file( GLOB STELLYRA_SOURCES
	${CMAKE_SOURCE_DIR}/../libs/stellyra/include/kmac/*.h
	${CMAKE_SOURCE_DIR}/../libs/stellyra/include/kmac/stellyra/*.h
	${CMAKE_SOURCE_DIR}/../libs/stellyra_extras/include/kmac/*.h
	${CMAKE_SOURCE_DIR}/../libs/stellyra_extras/include/kmac/stellyra/*.h
)

# Collect Stellyra documents
file( GLOB STELLYRA_DOCS
	${CMAKE_SOURCE_DIR}/../*.md
	${CMAKE_SOURCE_DIR}/../docs/*.md
)

add_executable( StellyraExamples
	example.cpp
	${STELLYRA_SOURCES}
	${STELLYRA_DOCS}
)

set( FILES_DIR ../libs/stellyra/include )
set( EXTRAS_DIR ../libs/stellyra_extras/include )
include_directories( ${FILES_DIR} ${EXTRAS_DIR} )

include( GNUInstallDirs )
install( TARGETS StellyraExamples
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
