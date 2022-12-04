#pragma once

#include <string>
#include <string_view>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_list.hpp>
using namespace userver;
namespace yet_another_disk {

    void AppendService(userver::components::ComponentList &component_list);

    bool CheckImport(const formats::json::Value &elem,
                     storages::postgres::Transaction &trx);

    bool CheckFile(const formats::json::Value &elem);

    bool CheckFolder(const formats::json::Value &elem);

}  // namespace yet_another_disk
