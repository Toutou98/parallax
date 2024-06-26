set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(${CMAKE_SOURCE_DIR}/lib)

if(PARALLAX_BUILD_TESTING)
  set(LIB_TEST_FILES
      tiresias.c
      manto.c
      test_recovery.c
      test_index_node.c
      test_leaf_node.c
      test_scans.c
      test_dirty_scans.c
      test_options.c
      test_categories.c
      test_sanitizers.c
      simple_test_delete.c
      test_gc.c
      test_leaf_root_delete_get_scan.c
      test_region_allocations.c
      test_par_format.c
      test_par_put_serialized.c
      test_wcursors_segment_buffers_cursor.c
      test_par_threadpool.c)

  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set_source_files_properties(${LIB_TEST_FILES} COMPILE_FLAGS "-O0 -g")
  else()
    set_source_files_properties(${LIB_TEST_FILES} COMPILE_FLAGS "-O3")
  endif()

  add_executable(test_index_node test_index_node.c arg_parser.c)
  target_link_libraries(test_index_node "${PROJECT_NAME}")

  add_executable(test_leaf_node test_leaf_node.c arg_parser.c)
  target_link_libraries(test_leaf_node "${PROJECT_NAME}")

  add_executable(test_scans test_scans.c)
  target_link_libraries(test_scans "${PROJECT_NAME}")

  add_executable(test_dirty_scans test_dirty_scans.c arg_parser.c)
  target_link_libraries(test_dirty_scans "${PROJECT_NAME}")

  add_executable(test_fractal_layout test_fractal_layout.c
                                     ../lib/btree/compaction/fractal_leaf.c)
  target_link_libraries(test_fractal_layout "${PROJECT_NAME}")

  find_library(BDB_LIBRARY NAMES libdb.so)

  if(BDB_LIBRARY AND NOT ${BDB_PATH} STREQUAL "")
    message("Tiresias and Manto tests will be built.")
    add_executable(tiresias tiresias.c arg_parser.c)
    target_link_libraries(tiresias "${PROJECT_NAME}" ${BDB_LIBRARY})
    add_parallax_test(
      NAME
      tiresias
      COMMAND
      run_test.sh
      tiresias
      --num_of_kvs=1000000
      --BDB_file=${BDB_PATH})

    add_executable(manto manto.c)
    target_link_libraries(manto "${PROJECT_NAME}" ${BDB_LIBRARY} ${ART_LIB})
    add_parallax_test(
      NAME
      manto
      COMMAND
      run_test.sh
      manto
      --num_of_kvs=16354
      --BDB_file=${BDB_PATH}
      --num_of_ssts=12)

  endif()

  add_parallax_test(NAME test_index_node COMMAND run_test.sh test_index_node)
  add_parallax_test(NAME test_leaf_node COMMAND run_test.sh test_leaf_node)
  add_parallax_test(NAME test_fractal_layout COMMAND run_test.sh
                    test_fractal_layout)

  add_parallax_test(
    NAME
    test_dirty_scans_sd_greater
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All_scan_greater
    --kv_mix=sd)

  add_parallax_test(
    NAME
    test_dirty_scans_small
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=s)

  add_parallax_test(
    NAME
    test_dirty_scans_medium
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=m)

  add_parallax_test(
    NAME
    test_dirty_scans_large
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=l)

  add_parallax_test(
    NAME
    test_dirty_scans_smalld
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=sd)

  add_parallax_test(
    NAME
    test_dirty_scans_mediumd
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=md)

  add_parallax_test(
    NAME
    test_dirty_scans_larged
    COMMAND
    run_test.sh
    test_dirty_scans
    --num_of_kvs=1000000
    --scan_size=5
    --workload=All
    --kv_mix=ld)

  add_parallax_test(NAME test_dirty_scans_smallp COMMAND run_test.sh
                    test_dirty_scans.sh s)
  add_parallax_test(NAME test_dirty_scans_mediump COMMAND run_test.sh
                    test_dirty_scans.sh m)
  add_parallax_test(NAME test_dirty_scans_largep COMMAND run_test.sh
                    test_dirty_scans.sh l)
  add_parallax_test(NAME test_dirty_scans_smalldp COMMAND run_test.sh
                    test_dirty_scans.sh sd)
  add_parallax_test(NAME test_dirty_scans_mediumdp COMMAND run_test.sh
                    test_dirty_scans.sh md)
  add_parallax_test(NAME test_dirty_scans_largedp COMMAND run_test.sh
                    test_dirty_scans.sh ld)

  add_executable(test_recovery test_recovery.c)
  target_link_libraries(test_recovery "${PROJECT_NAME}")

  add_executable(test_options test_options.c)
  target_link_libraries(test_options "${PROJECT_NAME}")
  add_parallax_test(NAME test_options COMMAND run_test.sh test_options)

  add_executable(test_categories test_categories.c)
  target_link_libraries(test_categories "${PROJECT_NAME}")
  add_parallax_test(NAME test_categories COMMAND run_test.sh test_categories)

  set_source_files_properties(test_sanitizers.c COMPILE_FLAGS "-O0")
  add_executable(test_sanitizers test_sanitizers.c)
  add_parallax_test(NAME test_sanitizers COMMAND run_test.sh test_sanitizers)

  add_executable(test_gc test_gc.c arg_parser.c)
  target_link_libraries(test_gc "${PROJECT_NAME}")
  add_parallax_test(NAME test_gc COMMAND run_test.sh test_gc
                    --num_of_kvs=1000000)

  if(TEBIS_FORMAT)
    add_test(NAME test_wcursors_segment_buffers_cursor
             COMMAND $<TARGET_FILE:test_wcursors_segment_buffers_cursor>
                     --file=${FILEPATH})
    add_executable(test_wcursors_segment_buffers_cursor
                   test_wcursors_segment_buffers_cursor.c arg_parser.c)
    target_link_libraries(test_wcursors_segment_buffers_cursor
                          "${PROJECT_NAME}" ${DEPENDENCIES})

    add_test(NAME test_wcursors_api COMMAND $<TARGET_FILE:test_wcursors_api>
                                            --file=${FILEPATH})
    add_executable(test_wcursors_api test_wcursors_api.c arg_parser.c)
    target_link_libraries(test_wcursors_api "${PROJECT_NAME}" ${DEPENDENCIES})

  endif()

  add_executable(test_mixes test_mixes.c arg_parser.c)
  target_link_libraries(test_mixes "${PROJECT_NAME}")

  add_parallax_test(
    NAME
    test_medium
    COMMAND
    run_test.sh
    test_mixes
    --num_of_kvs=400000
    --small_kv_percentage=0
    --medium_kv_percentage=100
    --big_kv_percentage=0)

  add_parallax_test(
    NAME
    test_mixes_99_small_1_medium
    COMMAND
    run_test.sh
    test_mixes
    --num_of_kvs=400000
    --small_kv_percentage=99
    --medium_kv_percentage=1
    --big_kv_percentage=0)

  add_parallax_test(
    NAME
    test_mixes_45_small_50_medium_5_big
    COMMAND
    run_test.sh
    test_mixes
    --num_of_kvs=400000
    --medium_kv_percentage=50
    --small_kv_percentage=45
    --big_kv_percentage=5)

  add_executable(simple_test_delete simple_test_delete.c arg_parser.c)
  target_link_libraries(simple_test_delete "${PROJECT_NAME}")
  add_parallax_test(NAME simple_test_delete COMMAND run_test.sh
                    simple_test_delete --num_of_kvs=1000000)

  add_executable(
    test_leaf_root_delete_get_scan test_leaf_root_delete_get_scan.c
                                   arg_parser.c ../lib/common/common.c)
  target_link_libraries(test_leaf_root_delete_get_scan "${PROJECT_NAME}")

  add_parallax_test(NAME test_leaf_root_delete_get_scan COMMAND run_test.sh
                    test_leaf_root_delete_get_scan)

  add_executable(
    test_region_allocations
    test_region_allocations.c ../lib/allocator/allocator.c
    ../lib/allocator/djb2.c ../lib/allocator/region_log.c)
  target_link_libraries(test_region_allocations "${PROJECT_NAME}")
  add_parallax_test(NAME test_region_allocations COMMAND run_test.sh
                    test_region_allocations /tmp/nvme/kv_store.dat)

  add_executable(
    test_region_log
    test_region_log.c ../lib/allocator/allocator.c ../lib/allocator/djb2.c
    ../lib/allocator/region_log.c arg_parser.c)
  target_link_libraries(test_region_log "${PROJECT_NAME}")
  add_parallax_test(NAME test_region_log COMMAND run_test.sh test_region_log
                    --num_threads=4)

  add_executable(test_optional_logging test_optional_logging.c)
  add_parallax_test(NAME test_optional_logging COMMAND run_test.sh
                    test_optional_logging)

  add_executable(test_wal test_wal.c)
  target_link_libraries(test_wal "${PROJECT_NAME}")

  add_executable(test_level_protocol test_level_protocol.c)
  target_link_libraries(test_level_protocol "${PROJECT_NAME}")

  add_executable(test_par_format test_par_format.c arg_parser.c)
  target_link_libraries(test_par_format "${PROJECT_NAME}")
  add_parallax_test(NAME test_par_format COMMAND run_test.sh test_par_format)

  add_executable(test_par_put_serialized test_par_put_serialized.c arg_parser.c)
  target_link_libraries(test_par_put_serialized "${PROJECT_NAME}")
  add_parallax_test(NAME test_par_put_serialized COMMAND run_test.sh
                    test_par_put_serialized)

  add_executable(test_par_put_metadata test_par_put_metadata.c arg_parser.c)
  target_link_libraries(test_par_put_metadata "${PROJECT_NAME}")
  add_parallax_test(NAME test_par_put_metadata COMMAND run_test.sh
                    test_par_put_metadata --num_of_kvs=400000)

  add_executable(test_par_threadpool test_threadpool.c
                                     ../lib/btree/compaction/threadpool.c)
  target_link_libraries(test_par_threadpool "${PROJECT_NAME}")
  add_parallax_test(NAME test_par_threadpool COMMAND run_test.sh
                    test_par_threadpool)

endif()
