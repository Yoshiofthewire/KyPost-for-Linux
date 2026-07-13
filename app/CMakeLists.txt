add_executable(llamamail
  main.cpp
  qml/qml.qrc
)

target_include_directories(llamamail PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llamamail PRIVATE
  llamacore
  Qt6::Quick
  Qt6::Qml
  Qt6::QuickControls2
  KF6::Kirigami
)

# SecureStoreKeychain talks to the Secret Service over QtDBus, so it belongs
# in the app executable, never in llamacore.
find_package(Qt6Keychain REQUIRED)
target_sources(llamamail PRIVATE
  platform/SecureStoreKeychain.cpp
  platform/SecureStoreKeychain.h
)
target_link_libraries(llamamail PRIVATE Qt6Keychain::Qt6Keychain)

include(GNUInstallDirs)
install(TARGETS llamamail RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
