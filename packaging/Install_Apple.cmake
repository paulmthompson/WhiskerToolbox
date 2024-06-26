


set(executable_path "$<TARGET_FILE_NAME:WhiskerToolbox>.app")
set(data_manager_path "lib/$<TARGET_FILE_NAME:DataManager>")

# Generate a deployment script to be executed at install time
qt_generate_deploy_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
        CONTENT "
   qt_deploy_runtime_dependencies(
   EXECUTABLE ${executable_path}
   ADDITIONAL_LIBRARIES ${data_manager_path}
   GENERATE_QT_CONF
   VERBOSE
   )")

install(TARGETS DataManager WhiskerToolbox
        BUNDLE DESTINATION .
)

install(SCRIPT "${deploy_script}")

