cmake_minimum_required(VERSION 3.16)

project(DukaanOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

# POS logic without any screens. Shared by the app and the tests, so a new
# .cpp file only has to be listed here once.
set(POS_LOGIC_SOURCES
    POS/common/data_paths.cpp
    POS/common/date_format.cpp
    POS/common/id_generator.cpp
    POS/common/record_codec.cpp
    POS/user/user_repository.cpp
    POS/user/auth_service.cpp
    POS/product/product_repository.cpp
    POS/product/product_service.cpp
    POS/product/barcode_image.cpp
    POS/order/order_repository.cpp
    POS/order/order_service.cpp
    POS/payment/payment_methods.cpp
    POS/bill/bill_repository.cpp
    POS/bill/bill_service.cpp
    POS/checkout/checkout_service.cpp
    BarCode/encoder/encoder.cpp
    BarCode/generator/generator.cpp
)

# Screens and camera scanning. Shared by the app and the UI smoke test.
set(DUKAANOS_UI_SOURCES
    ui/ui.cpp
    ui/theme.cpp
    ui/pages/login_page.cpp
    ui/pages/dashboard_page.cpp
    ui/pages/new_order_page.cpp
    ui/pages/held_orders_page.cpp
    ui/pages/order_history_page.cpp
    ui/pages/products_page.cpp
    ui/pages/product_dialog.cpp
    ui/pages/bill_page.cpp
    ui/pages/scan_product_page.cpp
    ui/pages/product_details_view.cpp
    ui/pages/saved_bill.cpp
    ${POS_LOGIC_SOURCES}
    POS/scanner/pos_barcode_scanner.cpp
    BarCode/scanner/scanner.cpp
    BarCode/decoder/decoder.cpp
    BarCode/camera/camera_scanner.cpp
)

set(DUKAANOS_ASSETS
    ui/assets/app_icon.png
    ui/assets/dukaanos_logo.png
    ui/assets/dukaanos_wordmark.png
    ui/assets/market_background.jpg
    ui/assets/checkbox_check.png
)

qt_add_executable(DukaanOS
    main.cpp
    ${DUKAANOS_UI_SOURCES}
)

qt_add_resources(DukaanOS app_assets
    PREFIX "/assets"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets"
    FILES ${DUKAANOS_ASSETS}
)

target_include_directories(DukaanOS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
target_link_libraries(DukaanOS PRIVATE Qt6::Widgets Qt6::Multimedia)
set_target_properties(DukaanOS PROPERTIES WIN32_EXECUTABLE ON)

include(CTest)
if(BUILD_TESTING)
    foreach(test_name PosStarterTest UserAuthTest BillCheckoutTest)
        if(test_name STREQUAL "PosStarterTest")
            set(test_source POS/tests/pos_starter_test.cpp)
        elseif(test_name STREQUAL "UserAuthTest")
            set(test_source POS/tests/user_auth_test.cpp)
        else()
            set(test_source POS/tests/bill_checkout_test.cpp)
        endif()
        qt_add_executable(${test_name} ${test_source} ${POS_LOGIC_SOURCES})
        target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
        target_link_libraries(${test_name} PRIVATE Qt6::Gui)
        set_target_properties(${test_name} PROPERTIES WIN32_EXECUTABLE OFF)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    qt_add_executable(CameraScannerTest
        BarCode/tests/barcode_roundtrip_test.cpp
        BarCode/encoder/encoder.cpp
        BarCode/generator/generator.cpp
        BarCode/decoder/decoder.cpp
        BarCode/scanner/scanner.cpp
        BarCode/camera/camera_scanner.cpp
    )
    target_include_directories(CameraScannerTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
    target_link_libraries(CameraScannerTest PRIVATE Qt6::Gui Qt6::Multimedia)
    add_test(NAME CameraScannerTest COMMAND CameraScannerTest)

    # Order and bill IDs, saving/reading orders, on-hold -> paid, payments.
    qt_add_executable(OrderRepositoryTest POS/tests/order_repository_test.cpp ${POS_LOGIC_SOURCES})
    target_include_directories(OrderRepositoryTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
    target_link_libraries(OrderRepositoryTest PRIVATE Qt6::Gui)
    set_target_properties(OrderRepositoryTest PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME OrderRepositoryTest COMMAND OrderRepositoryTest)

    # Reads the sample data in Data/ and checks it is valid and consistent.
    qt_add_executable(DataFilesTest POS/tests/data_files_test.cpp ${POS_LOGIC_SOURCES})
    target_include_directories(DataFilesTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
    target_compile_definitions(DataFilesTest PRIVATE DUKAANOS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(DataFilesTest PRIVATE Qt6::Gui)
    set_target_properties(DataFilesTest PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME DataFilesTest COMMAND DataFilesTest)

    # File names and formats of every data file, checked line by line against
    # the Increment 3 requirements. One test per file group; a group whose file
    # does not exist yet (PCategory.txt, Seasons.txt) is reported as Skipped.
    qt_add_executable(FileFormatTest POS/tests/file_format_test.cpp POS/tests/data_format_checker.cpp)
    target_compile_definitions(FileFormatTest PRIVATE DUKAANOS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(FileFormatTest PRIVATE Qt6::Core)
    set_target_properties(FileFormatTest PROPERTIES WIN32_EXECUTABLE OFF)
    foreach(format_group FileNames Users Products ProductCategories PCategory Orders Bills BillDetails Seasons)
        add_test(NAME FileFormat.${format_group} COMMAND FileFormatTest ${format_group})
        set_tests_properties(FileFormat.${format_group} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()

    # Opens every screen offscreen: login checks, role lock, light/dark mode,
    # and a cashier sale (hold, resume, pay). Uses a temporary copy of Data/.
    qt_add_executable(UiSmokeTest ui/tests/ui_smoke_test.cpp POS/tests/data_format_checker.cpp ${DUKAANOS_UI_SOURCES})
    qt_add_resources(UiSmokeTest ui_smoke_assets
        PREFIX "/assets"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets"
        FILES ${DUKAANOS_ASSETS}
    )
    target_include_directories(UiSmokeTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BarCode")
    target_compile_definitions(UiSmokeTest PRIVATE DUKAANOS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(UiSmokeTest PRIVATE Qt6::Widgets Qt6::Multimedia)
    set_target_properties(UiSmokeTest PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME UiSmokeTest COMMAND UiSmokeTest)
    set_tests_properties(UiSmokeTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
