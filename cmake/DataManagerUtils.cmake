
# Function to add prefix to all items in a list variable
function(prefix_list_items LIST_VAR PREFIX)
    set(prefixed_list "")
    foreach(item ${${LIST_VAR}})
        list(APPEND prefixed_list "${PREFIX}/${item}")
    endforeach()
    set(${LIST_VAR} ${prefixed_list} PARENT_SCOPE)
endfunction()

# Function to add test files to global property
# Usage: add_tests_to_global(test_list directory_path global_property)
function(add_tests_to_global TEST_LIST DIRECTORY_PATH GLOBAL_PROPERTY)
    set(tests_to_add "")
    foreach(test_file ${${TEST_LIST}})
        list(APPEND tests_to_add "${DIRECTORY_PATH}/${test_file}")
    endforeach()
    
    get_property(current_tests GLOBAL PROPERTY ${GLOBAL_PROPERTY})
    list(APPEND current_tests ${tests_to_add})
    set_property(GLOBAL PROPERTY ${GLOBAL_PROPERTY} "${current_tests}")
endfunction()