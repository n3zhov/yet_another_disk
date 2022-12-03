#include "hello.hpp"

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/datetime/date.hpp>
#include <userver/utils/assert.hpp>
#include <boost/algorithm/string.hpp>
using namespace userver;
namespace yet_another_disk {

    namespace {

        const std::string kFolder = "FOLDER";
        const std::string kFile = "FILE";

        std::optional<std::map<std::string, formats::json::Value>> GetJsonArgs(
                const std::string &request);

        bool CheckImport(const formats::json::Value &elem,
                         storages::postgres::Transaction &trx);

        bool CheckFile(const formats::json::Value &elem,
                         storages::postgres::Transaction &trx);

        bool CheckFolder(const formats::json::Value &elem,
                       storages::postgres::Transaction &trx);

        std::string getTypeById(const std::string &name,
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
                    auto trx = pg_cluster_->Begin(userver::storages::postgres::TransactionOptions{});
                    for(auto &elem: args["items"]){
                        CheckImport(elem, trx);
                    }
                    trx.Commit();
                } else {
                    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                }

                return {};
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

        bool CheckImport(const formats::json::Value &elem,
                      storages::postgres::Transaction &trx){
            const auto name = elem["id"].As<std::string>("");
            const auto importType = elem["type"].As<std::string>("");
            if(name.empty() || importType.empty()){
                return false;
            }

            std::string type = getTypeById(name, trx);

            if(!type.empty() && type != importType){
                return false;
            }

            const auto parentId = elem["parentId"].As<std::string>("");
            if(!parentId.empty() && getTypeById(parentId, trx) != kFolder){
                return false;
            }
            if(importType == kFolder){
                return CheckFolder(elem, trx);
            }
            else if(importType == kFile){
                return CheckFile(elem, trx);
            }
            else{
                return false;
            }
        }

        bool CheckFile(const formats::json::Value &elem,
                       storages::postgres::Transaction &trx){

        }

        bool CheckFolder(const formats::json::Value &elem,
                         storages::postgres::Transaction &trx){

        }


        std::string getTypeById(const std::string &name,
                                storages::postgres::Transaction &trx){
            const std::string& queryCheck = "SELECT\n"
                                            "\tid, item_type\n"
                                            "FROM\n"
                                            "\tyet_another_disk.system_items s\n"
                                            "WHERE\n"
                                            "    id=$1;";
            auto res = trx.Execute(queryCheck, name);
            std::string type;
            if(!res.IsEmpty()){
                type = res[0]["item_type"].As<std::string>();
                boost::algorithm::trim_right(type);
            }
            return type;
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
