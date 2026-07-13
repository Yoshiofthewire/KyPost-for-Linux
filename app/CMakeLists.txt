add_executable(llamamail
  main.cpp
  qml/qml.qrc
)

target_link_libraries(llamamail PRIVATE
  llamacore
  ${QT_TARGET_PREFIX}::Quick
  ${QT_TARGET_PREFIX}::Qml
  ${QT_TARGET_PREFIX}::QuickControls2
  ${KIRIGAMI_TARGET}
)
