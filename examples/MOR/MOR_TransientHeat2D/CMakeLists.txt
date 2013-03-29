if (ALBANY_SEACAS)
  # 1. Copy Input files from source to binary dir
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_galerkin_exo.xml
                 ${CMAKE_CURRENT_BINARY_DIR}/input_galerkin_exo.xml COPYONLY)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fullpodbasis.in.exo.4.0
                 ${CMAKE_CURRENT_BINARY_DIR}/fullpodbasis.in.exo.4.0 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fullpodbasis.in.exo.4.1
                 ${CMAKE_CURRENT_BINARY_DIR}/fullpodbasis.in.exo.4.1 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fullpodbasis.in.exo.4.2
                 ${CMAKE_CURRENT_BINARY_DIR}/fullpodbasis.in.exo.4.2 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fullpodbasis.in.exo.4.3
                 ${CMAKE_CURRENT_BINARY_DIR}/fullpodbasis.in.exo.4.3 COPYONLY)

  # 2. Name the test with the directory name
  get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

  # 3. Create the test with this name and standard executable
  add_test(${testName}_galerkin_exo ${Albany.exe} input_galerkin_exo.xml)
endif (ALBANY_SEACAS)