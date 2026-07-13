# One executable per QtTest class, registered with ctest. Task 7 does a
# final consistency pass once Tasks 4-6 add their own tests here.

function(llama_add_test test_name)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} PRIVATE llamacore ${QT_TARGET_PREFIX}::Test)
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

llama_add_test(EmailTest core/models/EmailTest.cpp)
llama_add_test(ContactTest core/models/ContactTest.cpp)
llama_add_test(StandardFolderTest core/models/StandardFolderTest.cpp)
llama_add_test(KeywordSettingsTest core/models/KeywordSettingsTest.cpp)
llama_add_test(MfaChallengeTest core/models/MfaChallengeTest.cpp)
llama_add_test(PushNotificationTest core/models/PushNotificationTest.cpp)
