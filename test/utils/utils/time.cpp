// Copyright 2022 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils/time.hpp"

#include <chrono>

namespace test
{
std::chrono::system_clock::time_point ten_milliseconds_from_now()
{
    return std::chrono::system_clock::now() + std::chrono::milliseconds(10);
}

std::chrono::system_clock::time_point hundred_milliseconds_from_now()
{
    return std::chrono::system_clock::now() + std::chrono::milliseconds(100);
}

std::chrono::system_clock::time_point one_seconds_from_now()
{
    return std::chrono::system_clock::now() + std::chrono::seconds(1);
}

std::chrono::system_clock::time_point five_seconds_from_now()
{
    return std::chrono::system_clock::now() + std::chrono::seconds(5);
}
}  // namespace test
