#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace yet_another_disk {

    void AppendService(userver::components::ComponentList &component_list);

}  // namespace yet_another_disk
