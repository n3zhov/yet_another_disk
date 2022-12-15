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
#include <nlohmann/json.hpp>
using namespace userver;
namespace yet_another_disk {
    const std::string kFolder = "FOLDER";
    const std::string kFile = "FILE";
    const boost::uuids::name_generator uuidGen(boost::uuids::ns::x500dn());


    bool checkFile(const formats::json::Value &elem);

    bool checkFolder(const formats::json::Value &elem);

    std::optional<std::map<std::string, formats::json::Value>> getJsonArgs(
            const formats::json::Value &request_json);

    std::optional<storages::postgres::ResultSet> checkImport(const formats::json::Value &elem,
                                                             storages::postgres::Transaction &trx);

    storages::postgres::ResultSet getItemById(const boost::uuids::uuid &id,
                                              storages::postgres::Transaction &trx);

    std::optional<formats::json::Value> getItemAndChildren(const boost::uuids::uuid &uid, const std::string &id,
                                              storages::postgres::Transaction &trx);

    nlohmann::json parseRes (const storages::postgres::ResultSet &res, const std::string &resId);

    nlohmann::json parseRow(const storages::postgres::Row &row);

    void updateParent(const boost::uuids::uuid &id, long long changeSize, storages::postgres::TimePointTz,
                      storages::postgres::Transaction &trx);

    void insertItem(const formats::json::Value &elem,
                    const userver::storages::postgres::TimePointTz &date,
                    storages::postgres::Transaction &trx);

    std::string getStringFromField(const storages::postgres::Field &elem);

    template <typename T>
    void createOptional (const storages::postgres::Field &elem, std::optional<T> &res);

    formats::json::Value notFound(const server::http::HttpRequest &request);

    void deleteElemById(const boost::uuids::uuid &id,
                                              storages::postgres::Transaction &trx);

    void AppendService(userver::components::ComponentList &component_list);

}  // namespace yet_another_disk
