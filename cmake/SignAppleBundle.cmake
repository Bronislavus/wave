if(NOT BUILD_CONFIG STREQUAL "Release")
    return()
endif()

foreach(REQUIRED_VALUE CODESIGN_EXECUTABLE CODESIGN_IDENTITY BUNDLE_PATH)
    if(NOT DEFINED ${REQUIRED_VALUE} OR "${${REQUIRED_VALUE}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${REQUIRED_VALUE} for macOS bundle signing")
    endif()
endforeach()

execute_process(
    COMMAND "${CODESIGN_EXECUTABLE}"
        --force
        --deep
        --timestamp
        --options runtime
        --sign "${CODESIGN_IDENTITY}"
        "${BUNDLE_PATH}"
    RESULT_VARIABLE SIGN_RESULT)
if(NOT SIGN_RESULT EQUAL 0)
    message(FATAL_ERROR "Developer ID signing failed for ${BUNDLE_PATH}")
endif()

execute_process(
    COMMAND "${CODESIGN_EXECUTABLE}"
        --verify
        --deep
        --strict
        --verbose=2
        "${BUNDLE_PATH}"
    RESULT_VARIABLE VERIFY_RESULT)
if(NOT VERIFY_RESULT EQUAL 0)
    message(FATAL_ERROR "Signature verification failed for ${BUNDLE_PATH}")
endif()
