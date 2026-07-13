add_library(llamacore STATIC
  models/StandardFolder.cpp
  models/Email.h
  models/Contact.h
  models/StandardFolder.h
  models/KeywordSettings.h
  models/MfaChallenge.h
  models/PushNotification.h
)

target_include_directories(llamacore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llamacore PUBLIC
  ${QT_TARGET_PREFIX}::Core
  ${QT_TARGET_PREFIX}::Network
  ${QT_TARGET_PREFIX}::Sql
)
