#pragma once

#include <string>
#include <string_view>
#include <userver/storages/postgres/component.hpp>
#include <userver/components/component_list.hpp>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <fmt/format.h>
#include <userver/clients/dns/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime/date.hpp>
#include <boost/algorithm/string.hpp>

using namespace userver;
namespace yet_another_disk {
    const std::string kFolder = "FOLDER";
    const std::string kFile = "FILE";
    const boost::uuids::string_generator uuidGen;


    bool CheckFile(const formats::json::Value &elem);

    bool CheckFolder(const formats::json::Value &elem);

    std::optional<std::map<std::string, formats::json::Value>> GetJsonArgs(
            const std::string &request);

    std::optional<storages::postgres::ResultSet> CheckImport(const formats::json::Value &elem,
                                                             storages::postgres::Transaction &trx);

    storages::postgres::ResultSet getItemById(const boost::uuids::uuid &id,
                                              storages::postgres::Transaction &trx);

    std::optional<formats::json::Value> getItemAndChildren(const boost::uuids::uuid &uid, const std::string &id,
                                              storages::postgres::Transaction &trx);

    formats::json::Value parseRes (const storages::postgres::ResultSet &res, const std::string &resId);

    formats::json::Value parseRow(const storages::postgres::Row &row);

    void UpdateParentSize(const boost::uuids::uuid &id, long long changeSize,
                          storages::postgres::Transaction &trx);

    void InsertItem(const formats::json::Value &elem,
                    const userver::storages::postgres::TimePointTz &date,
                    storages::postgres::Transaction &trx);

    std::string getStringFromField(const storages::postgres::Field &elem);

    void AppendService(userver::components::ComponentList &component_list);

}  // namespace yet_another_disk
