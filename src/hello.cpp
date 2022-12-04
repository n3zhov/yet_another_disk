#include "hello.hpp"

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime/date.hpp>
#include <userver/utils/assert.hpp>
#include <boost/algorithm/string.hpp>

namespace yet_another_disk {

    namespace {

        const std::string kFolder = "FOLDER";
        const std::string kFile = "FILE";

        std::optional<std::map<std::string, formats::json::Value>> GetJsonArgs(
                const std::string &request);

        std::optional<storages::postgres::ResultSet> CheckImport(const formats::json::Value &elem,
                         storages::postgres::Transaction &trx);

        bool CheckFile(const formats::json::Value &elem);

        bool CheckFolder(const formats::json::Value &elem);

        storages::postgres::ResultSet getItemById(const std::string &id,
                         storages::postgres::Transaction &trx);

        void UpdateParentSize(const std::string &id, long long changeSize,
                              storages::postgres::Transaction &trx);

        void InsertItem(const formats::json::Value &elem,
                        const userver::storages::postgres::TimePointTz &date,
                        storages::postgres::Transaction &trx);

        class Imports final : public server::handlers::HttpHandlerBase {
        public:
            static constexpr std::string_view kName = "handler-imports";

            Imports(const components::ComponentConfig &config,
                    const components::ComponentContext &component_context)
                    : HttpHandlerBase(config, component_context),
                      pg_cluster_(
                              component_context
                                      .FindComponent<components::Postgres>("postgres-db-1")
                                      .GetCluster()) {}

            std::string HandleRequestThrow(
                    const server::http::HttpRequest &request,
                    server::request::RequestContext &) const override {
                auto &response = request.GetHttpResponse();

                response.SetContentType("text/plain");
                const std::string &body = request.RequestBody();
                auto parsed = GetJsonArgs(body);
                if (parsed.has_value()) {
                    auto &args = parsed.value();
                    const auto date = args["updateDate"].As<storages::postgres::TimePointTz>();
                    auto trx = pg_cluster_->Begin(userver::storages::postgres::TransactionOptions{});
                    for(auto &elem: args["items"]){
                        auto prevValue = CheckImport(elem, trx);
                        if(!prevValue.has_value()){
                            request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                            return {};
                        }
                        else{
                            const auto &prevElem = prevValue.value();
                            if(!prevElem.IsEmpty() && !prevElem[0]["parent_id"].IsNull()){
                                UpdateParentSize(
                                        boost::algorithm::trim_right_copy(prevElem[0]["parent_id"].As<std::string>()),
                                        -prevElem[0]["item_size"].As<long long>(), trx);
                            }
                            InsertItem(elem, date, trx);
                        }
                    }
                    trx.Commit();
                    request.SetResponseStatus(server::http::HttpStatus::kOk);
                    return {};
                } else {
                    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                    return {};
                }
            }

            std::string query =
                    "INSERT INTO warehouse.socks ( type_id, color, cotton_part, quantity) "
                    "VALUES ( $1, $2, $3, $4 ) ON CONFLICT (type_id) DO UPDATE\n"
                    "SET quantity=socks.quantity + excluded.quantity";
            storages::postgres::ClusterPtr pg_cluster_;
        };

        class Nodes final : public server::handlers::HttpHandlerBase {
        public:
            static constexpr std::string_view kName = "handler-nodes";

            Nodes(const components::ComponentConfig &config,
                  const components::ComponentContext &component_context)
                    : HttpHandlerBase(config, component_context),
                      pg_cluster_(
                              component_context
                                      .FindComponent<components::Postgres>("postgres-db-1")
                                      .GetCluster()) {}

            std::string HandleRequestThrow(
                    const server::http::HttpRequest &request,
                    server::request::RequestContext &) const override {
                auto &response = request.GetHttpResponse();

                response.SetContentType("text/plain");
                const std::string &body = request.RequestBody();
                const auto arg = request.GetPathArg("id");
                auto parsed = GetJsonArgs(body);
                if (parsed.has_value()) {
                    auto &args = parsed.value();
                    pg_cluster_->Execute(
                            storages::postgres::ClusterHostType::kMaster, query,
                            args["color"].As<std::string>() +
                            std::to_string(args["cottonPart"].As<int>()),
                            args["color"].As<std::string>(), args["cottonPart"].As<int>(),
                            args["quantity"].As<int>());
                } else {
                    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                }

                return {};
            }

            std::string query =
                    "INSERT INTO warehouse.socks ( type_id, color, cotton_part, quantity) "
                    "VALUES ( $1, $2, $3, $4 ) ON CONFLICT (type_id) DO UPDATE\n"
                    "SET quantity=socks.quantity - excluded.quantity";
            storages::postgres::ClusterPtr pg_cluster_;
        };

        class Delete final : public server::handlers::HttpHandlerBase {
        public:
            static constexpr std::string_view kName = "handler-delete";

            Delete(const components::ComponentConfig &config,
                   const components::ComponentContext &component_context)
                    : HttpHandlerBase(config, component_context),
                      pg_cluster_(
                              component_context
                                      .FindComponent<components::Postgres>("postgres-db-1")
                                      .GetCluster()) {}

            std::string HandleRequestThrow(
                    const server::http::HttpRequest &request,
                    server::request::RequestContext &) const override {
                auto &response = request.GetHttpResponse();

                response.SetContentType("text/plain");
                const std::string &body = request.RequestBody();
                auto parsed = GetJsonArgs(body);
                if (parsed.has_value()) {
                    auto &args = parsed.value();
                    pg_cluster_->Execute(
                            storages::postgres::ClusterHostType::kMaster, query,
                            args["color"].As<std::string>() +
                            std::to_string(args["cottonPart"].As<int>()),
                            args["color"].As<std::string>(), args["cottonPart"].As<int>(),
                            args["quantity"].As<int>());
                } else {
                    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                }

                return {};
            }

            std::string query =
                    "INSERT INTO warehouse.socks ( type_id, color, cotton_part, quantity) "
                    "VALUES ( $1, $2, $3, $4 ) ON CONFLICT (type_id) DO UPDATE\n"
                    "SET quantity=socks.quantity - excluded.quantity";
            storages::postgres::ClusterPtr pg_cluster_;
        };

        std::optional<std::map<std::string, formats::json::Value>> GetJsonArgs(
                const std::string &request) {
            try {
                const auto &request_json = formats::json::FromString(request);
                //utils::datetime::
                std::map<std::string, formats::json::Value> res;
                res["updateDate"] = request_json["updateDate"];
                res["items"] = request_json["items"];
                return res;
            } catch (...) {
                return {};
            }
        }

        std::optional<storages::postgres::ResultSet> CheckImport(const formats::json::Value &elem,
                      storages::postgres::Transaction &trx){
            const auto name = elem["id"].As<std::string>("");
            const auto importType = elem["type"].As<std::string>("");
            if(name.empty() || importType.empty()){
                return {};
            }

            auto getItemType = [](const storages::postgres::ResultSet &arg){
                if(!arg.IsEmpty() && !arg[0]["item_type"].IsNull())
                    return boost::algorithm::trim_right_copy(arg[0]["item_type"].As<std::string>());
                else
                    return std::string();
            };
            auto res = getItemById(name, trx);
            std::string type = getItemType(res);
            if(!type.empty() && type != importType){
                return {};
            }

            const auto parentId = elem["parentId"].As<std::string>("");
            auto parentRes = getItemById(parentId, trx);
            std::string parent_type = getItemType(parentRes);
            if(!parentId.empty() && parent_type != kFolder){
                return {};
            }
            if(importType == kFolder){
                if(CheckFolder(elem))
                    return res;
                else
                    return {};
            }
            else if(importType == kFile){
                if(CheckFile(elem))
                    return res;
                else
                    return {};
            }
            else{
                return {};
            }
        }

        bool CheckFile(const formats::json::Value &elem){
            const auto url = elem["url"].As<std::string>("");
            const int size = elem["size"].As<int>(-1);
            if(!url.empty() && url.size() <= 255 && size > 0){
                return true;
            }
            else{
                return false;
            }
        }

        bool CheckFolder(const formats::json::Value &elem){
            if(elem["size"].IsEmpty() && elem["url"].IsEmpty()){
                return true;
            }
            else{
                return false;
            }
        }

        void UpdateParentSize(const std::string &id, const long long changeSize,
                              storages::postgres::Transaction &trx){
            const static std::string updateQuery = "WITH RECURSIVE r AS (\n"
                                                   "   SELECT id, parent_id, item_size, item_type\n"
                                                   "   FROM yet_another_disk.system_items\n"
                                                   "   WHERE id = $1\n"
                                                   "   UNION\n"
                                                   "   SELECT items.id, items.parent_id, items.item_size, items.item_type\n"
                                                   "   FROM yet_another_disk.system_items as items\n"
                                                   "      JOIN r\n"
                                                   "          ON items.id = r.parent_id\n"
                                                   ")\n"
                                                   "UPDATE yet_another_disk.system_items items\n"
                                                   "    SET item_size = item_size + $2\n"
                                                   "    WHERE items.id in (SELECT id FROM r) ;";


            trx.Execute(updateQuery, id, changeSize);
        }

        storages::postgres::ResultSet getItemById(const std::string &id,
                                storages::postgres::Transaction &trx){
            const static std::string& query = "SELECT\n"
                                            "\tid, item_type, item_size, parent_id, url, \"date-time\"\n"
                                            "FROM\n"
                                            "\tyet_another_disk.system_items s\n"
                                            "WHERE\n"
                                            "    id=$1;";
            return trx.Execute(query, id);
        }

        void InsertItem(const formats::json::Value &elem,
                        const userver::storages::postgres::TimePointTz &date,
                        storages::postgres::Transaction &trx){
            const static std::string insertItem = "INSERT INTO yet_another_disk.system_items\n"
                                                  "\t( id, url, parent_id, item_type, item_size, \"date-time\") "
                                                  "VALUES ( $1, $2, $3, $4, $5, $6)\n"
                                                  "ON CONFLICT (id) DO UPDATE\n"
                                                  "    SET id=excluded.id,\n"
                                                  "           url=excluded.url,\n"
                                                  "           parent_id=excluded.parent_id,\n"
                                                  "           item_type=excluded.item_type,\n"
                                                  "           item_size=excluded.item_size,\n"
                                                  "           \"date-time\"=excluded.\"date-time\"";
            const static std::string insertStory = "INSERT INTO yet_another_disk.history\n"
                                                   "\t( item_id, url, parent_id, item_type, item_size, \"date-time\") "
                                                   "VALUES ( $1, $2, $3, $4, $5, $6 );";
            
            const auto type = elem["type"].As<std::string>();

            auto execLambda = [date](const std::string &query,
                                const formats::json::Value &elem,
                                storages::postgres::Transaction &trx){
                trx.Execute(query, elem["id"].As<std::string>(), elem["url"].As<std::string>(""),
                            elem["parentId"].As<std::string>(""), elem["type"].As<std::string>(""),
                            elem["size"].As<long long>(0),
                            date);
            };

            execLambda(insertItem, elem, trx);
            if(elem["type"].As<std::string>() == kFile){
                execLambda(insertStory, elem, trx);
                const auto parent = elem["parentId"].As<std::string>("");
                if(!parent.empty()){
                    UpdateParentSize(parent, elem["size"].As<long long>(), trx);
                }
            }



        }
    }  // namespace

    void AppendService(components::ComponentList &component_list) {
        component_list.Append<components::Postgres>("postgres-db-1");
        component_list.Append<clients::dns::Component>();
        component_list.Append<Imports>();
        component_list.Append<Nodes>();
        component_list.Append<Delete>();
    }

}  // namespace yet_another_disk
