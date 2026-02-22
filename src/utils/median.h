#pragma once

#include <algorithm>

template<typename TContainer>
static double median(TContainer values) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    size_t size = values.size();

    if (size % 2 == 0) {
        return (values[size/2 - 1] + values[size/2]) / 2.0;
    } else {
        return values[size/2];
    }
}