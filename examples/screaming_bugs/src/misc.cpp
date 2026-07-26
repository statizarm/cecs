#include <cstddef>
#include <new>

// Реализация обычного многопараметрического оператора new[]
void* operator new
    [](size_t size, const char* /*pName*/, int /*flags*/,
       unsigned /*debugFlags*/, const char* /*file*/, int /*line*/) {
    return ::operator new[](size);
}

// Реализация выравнивающего оператора new[] (требуется для хэш-таблиц и
// SOA-контейнеров)
void* operator new
    [](size_t size, size_t alignment, size_t alignmentOffset,
       const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
       const char* /*file*/, int /*line*/) {
    // В C++17 доступен std::align_val_t для выровненного выделения памяти
    return ::operator new[](size, std::align_val_t(alignment));
}