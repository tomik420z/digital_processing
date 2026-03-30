cmake_minimum_required(VERSION 3.10)
project(EchoSignalProtection)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск необходимых пакетов
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(Boost REQUIRED COMPONENTS headers)
find_package(GTest REQUIRED)

# Определить корневой путь проекта
get_filename_component(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
add_compile_definitions(ROOT_PATH="${ROOT_PATH}")

# Включить директории с заголовочными файлами
include_directories(src)

# Создать библиотеку с алгоритмами фильтрации
set(FILTER_SOURCES
    src/signal_processor.cpp
    src/median_filter.cpp
    src/wiener_filter.cpp
    src/robust_wiener_filter.cpp
    src/morphological_filter.cpp
    src/outlier_detection.cpp
    src/savgol_filter.cpp
    src/kalman_filter.cpp
    src/signal_generator.cpp
    src/performance_tester.cpp
    src/signal_classifier.cpp
    src/adaptive_filter_selector.cpp
    src/spectral_subtraction_filter.cpp
    src/utils/linear_system_solver.cpp
)

set(FILTER_HEADERS
    src/signal_processor.h
    src/median_filter.h
    src/wiener_filter.h
    src/robust_wiener_filter.h
    src/morphological_filter.h
    src/outlier_detection.h
    src/savgol_filter.h
    src/kalman_filter.h
    src/signal_generator.h
    src/performance_tester.h
    src/signal_classifier.h
    src/adaptive_filter_selector.h
    src/spectral_subtraction_filter.h
    src/utils/linear_system_solver.h
    src/utils/median.h
    src/utils/fft.h
)

add_library(echo_filters STATIC ${FILTER_SOURCES} ${FILTER_HEADERS})
target_link_libraries(echo_filters PUBLIC Boost::headers)

# Основная программа тестирования
add_executable(echo_filter_test src/main.cpp)
target_link_libraries(echo_filter_test echo_filters Threads::Threads)

# Программа для генерации тестовых данных
add_executable(generate_test_data src/generate_data.cpp)
target_link_libraries(generate_test_data echo_filters)

# Генератор расширенных составных нелинейных сигналов (4096 точек)
add_executable(generate_extended_data src/generate_extended_data.cpp)
target_link_libraries(generate_extended_data echo_filters)
target_compile_options(generate_extended_data PRIVATE -O2 -Wall -Wextra)

# Генератор специализированных сигналов для фильтра Винера
# Все сигналы: медленно меняющийся истинный сигнал + высокочастотные/импульсные помехи
add_executable(generate_wiener_data src/generate_wiener_data.cpp)
target_link_libraries(generate_wiener_data echo_filters)
target_compile_options(generate_wiener_data PRIVATE -O2 -Wall -Wextra)

# GUI программа для визуализации фильтров
set(GUI_SOURCES
    view/main_gui.cpp
    view/signal_visualizer.cpp
    view/stb_image.cpp
)

set(GUI_HEADERS
    view/signal_visualizer.h
)

add_executable(signal_filter_gui ${GUI_SOURCES} ${GUI_HEADERS})
target_link_libraries(signal_filter_gui echo_filters Threads::Threads OpenGL::GL glfw GLEW::GLEW)

# Компиляционные флаги для оптимизации
target_compile_options(echo_filters PRIVATE -O2 -Wall -Wextra)
target_compile_options(echo_filter_test PRIVATE -O2 -Wall -Wextra)
target_compile_options(generate_test_data PRIVATE -O2 -Wall -Wextra)
target_compile_options(signal_filter_gui PRIVATE -O2 -Wall -Wextra)

# Тест для KalmanFilter (Google Test)
add_executable(test_kalman tests/test_kalman.cpp)
target_link_libraries(test_kalman echo_filters GTest::gtest GTest::gtest_main)

# Бенчмарк: одиночные фильтры vs двухэтапные цепочки (outlier → filter)
add_executable(pipeline_benchmark src/pipeline_benchmark.cpp)
target_link_libraries(pipeline_benchmark echo_filters Threads::Threads)
target_compile_options(pipeline_benchmark PRIVATE -O2 -Wall -Wextra)

# Установка целей
install(TARGETS echo_filter_test generate_test_data signal_filter_gui pipeline_benchmark
        RUNTIME DESTINATION bin)