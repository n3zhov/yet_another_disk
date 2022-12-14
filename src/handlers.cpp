#include "handlers.hpp"
#include "userver/formats/json/string_builder.hpp"
#include "userver/server/handlers/http_handler_json_base.hpp"



#include <userver/clients/dns/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <boost/algorithm/string.hpp>


namespace yet_another_disk {

    class Imports final : public server::handlers::HttpHandlerJsonBase {
    public:
        static constexpr std::string_view kName = "handler-imports";

        Imports(const components::ComponentConfig &config,
                const components::ComponentContext &component_context)
                : HttpHandlerJsonBase(config, component_context),
                  pg_cluster_(
                          component_context
                                  .FindComponent<components::Postgres>("postgres-db-1")
                                  .GetCluster()) {}

        formats::json::Value HandleRequestJsonThrow(
                const server::http::HttpRequest &request, const formats::json::Value& json,
                server::request::RequestContext &) const override {

            auto parsed = getJsonArgs(json);
            auto validationFailed = [&request](){
                request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
                return formats::json::MakeObject(
                        "code", 400,
                        "message", "Validation failed"
                );
            };

            if (parsed.has_value()) {
                auto &args = parsed.value();
                const auto date = args["updateDate"].As<storages::postgres::TimePointTz>();
                auto trx = pg_cluster_->Begin(userver::storages::postgres::TransactionOptions{});
                for (auto &elem: args["items"]) {
                    auto prevValue = checkImport(elem, trx);
                    if (!prevValue.has_value()) {
                        return validationFailed();
                    } else {
                        const auto &prevElem = prevValue.value();
                        if (!prevElem.IsEmpty() && !prevElem[0]["parent_id"].IsNull()) {
                            const auto parentId = uuidGen(getStringFromField(prevElem[0]["parent_id"]));
                            updateParent(
                                    parentId,
                                    -prevElem[0]["item_size"].As<long long>(), date, trx);
                        }
                        insertItem(elem, date, trx);
                    }
                }
                trx.Commit();
                request.SetResponseStatus(server::http::HttpStatus::kOk);
                return {};
            } else {
                request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
                return validationFailed();
            }
        }

        storages::postgres::ClusterPtr pg_cluster_;
    };

    class Nodes final : public server::handlers::HttpHandlerJsonBase {
    public:
        static constexpr std::string_view kName = "handler-nodes";

        Nodes(const components::ComponentConfig &config,
              const components::ComponentContext &component_context)
                : HttpHandlerJsonBase(config, component_context),
                  pg_cluster_(
                          component_context
                                  .FindComponent<components::Postgres>("postgres-db-1")
                                  .GetCluster()) {}

        formats::json::Value HandleRequestJsonThrow(
                const server::http::HttpRequest &request, const formats::json::Value& json,
                server::request::RequestContext &) const override {
            const std::string id = request.GetPathArg("id");
            const auto uId = uuidGen(id);
            auto trx = pg_cluster_->Begin(userver::storages::postgres::TransactionOptions{});
            const auto res = getItemAndChildren(uId, id, trx);
            if(res.has_value()){
                return res.value();
            }
            else {
                return notFound(request);
            }
        }

        storages::postgres::ClusterPtr pg_cluster_;
    };

    class Delete final : public server::handlers::HttpHandlerJsonBase {
    public:
        static constexpr std::string_view kName = "handler-delete";

        Delete(const components::ComponentConfig &config,
               const components::ComponentContext &component_context)
                : HttpHandlerJsonBase(config, component_context),
                  pg_cluster_(
                          component_context
                                  .FindComponent<components::Postgres>("postgres-db-1")
                                  .GetCluster()) {}

        formats::json::Value HandleRequestJsonThrow(
                const server::http::HttpRequest &request, const formats::json::Value& json,
                server::request::RequestContext &) const override {
            const std::string id = request.GetPathArg("id");
            const auto dateString = request.GetArg("date");
            if(dateString.empty()){
                request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
                return formats::json::MakeObject(
                        "code", 400,
                        "message", "Validation failed"
                );
            }
            const auto date = utils::datetime::Stringtime(request.GetArg("date"));
            const auto uId = uuidGen(id);
            auto trx = pg_cluster_->Begin(userver::storages::postgres::TransactionOptions{});
            auto res = getItemById(uId, trx);
            if(!res.IsEmpty()) {
                deleteElemById(uId, trx);
                trx.Commit();
                return {};
            }
            else{
                return notFound(request);
            }
        }
        storages::postgres::ClusterPtr pg_cluster_;
    };

    std::optional<std::map<std::string, formats::json::Value>> getJsonArgs(
            const formats::json::Value &request_json) {
        try {
            std::map<std::string, formats::json::Value> res;
            res["updateDate"] = request_json["updateDate"];
            res["items"] = request_json["items"];
            return res;
        } catch (...) {
            return {};
        }
    }

    std::optional<storages::postgres::ResultSet> checkImport(const formats::json::Value &elem,
                                                             storages::postgres::Transaction &trx) {
        const auto name = elem["id"].As<std::string>("");
        const auto importType = elem["type"].As<std::string>("");
        if (name.empty() || importType.empty()) {
            return {};
        }

        auto getItemType = [](const storages::postgres::ResultSet &arg) {
            if (!arg.IsEmpty() && !arg[0]["item_type"].IsNull()) {
                storages::postgres::Field res = arg[0]["item_type"];
                return boost::algorithm::trim_right_copy(arg[0]["item_type"].As<std::string>());
            } else
                return std::string();
        };
        auto res = getItemById(uuidGen(name), trx);
        std::string type = getItemType(res);
        if (!type.empty() && type != importType) {
            return {};
        }

        const auto parentId = elem["parentId"].As<std::string>("");
        auto parentRes = getItemById(uuidGen(parentId), trx);
        std::string parent_type = getItemType(parentRes);
        if (!parentId.empty() && parent_type != kFolder) {
            return {};
        }
        if (importType == kFolder) {
            if (checkFolder(elem))
                return res;
            else
                return {};
        } else if (importType == kFile) {
            if (checkFile(elem))
                return res;
            else
                return {};
        } else {
            return {};
        }
    }

    bool checkFile(const formats::json::Value &elem) {
        const auto url = elem["url"].As<std::string>("");
        const int size = elem["size"].As<int>(-1);
        if (!url.empty() && url.size() <= 255 && size > 0) {
            return true;
        } else {
            return false;
        }
    }

    bool checkFolder(const formats::json::Value &elem) {
        if (elem["size"].IsMissing() && elem["url"].IsMissing()) {
            return true;
        } else {
            return false;
        }
    }

    void updateParent(const boost::uuids::uuid &id, long long changeSize, storages::postgres::TimePointTz date,
                      storages::postgres::Transaction &trx) {
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
                                               "    SET item_size = item_size + $2,\n"
                                               "        \"date-time\" = $3\n"
                                               "    WHERE items.id in (SELECT id FROM r) ;";


        trx.Execute(updateQuery, id, changeSize, date);
    }

    storages::postgres::ResultSet getItemById(const boost::uuids::uuid &id,
                                              storages::postgres::Transaction &trx) {
        const static std::string &query = "SELECT\n"
                                          "\ts.id, s.item_type, s.item_size, s.parent_id, s.url, s.\"date-time\"\n"
                                          "FROM\n"
                                          "\tyet_another_disk.system_items s\n"
                                          "WHERE\n"
                                          "    id=$1;";
        return trx.Execute(query, id);
    }

    void insertItem(const formats::json::Value &elem,
                    const userver::storages::postgres::TimePointTz &date,
                    storages::postgres::Transaction &trx) {
        const static std::string insertItem = "INSERT INTO yet_another_disk.system_items\n"
                                              "\t( id_string, parent_string, id, url, parent_id, item_type, item_size, \"date-time\") "
                                              "VALUES ( $1, $2, $3, $4, $5, $6, $7, $8)\n"
                                              "ON CONFLICT (id) DO UPDATE\n"
                                              "    SET id=excluded.id,\n"
                                              "           url=excluded.url,\n"
                                              "           parent_id=excluded.parent_id,\n"
                                              "           item_type=excluded.item_type,\n"
                                              "           item_size=excluded.item_size,\n"
                                              "           \"date-time\"=excluded.\"date-time\"";
        const static std::string insertStory = "INSERT INTO yet_another_disk.history\n"
                                               "\t( item_id, url, parent_id, \"date-time\") "
                                               "VALUES ( $1, $2, $3, $4);";

        const auto type = elem["type"].As<std::string>();

        const auto id = elem["id"].As<std::string>();
        const auto uId = uuidGen(id);
        const auto parent = elem["parentId"].As<std::string>("");
        std::optional<boost::uuids::uuid> uParent;
        std::optional<std::string> parentId;
        if(!parent.empty()){
            uParent = uuidGen(parent);
            parentId = parent;
        }

        std::optional<std::string> url = elem["url"].As<std::string>("");
        if(url.value().empty())
            url = std::nullopt;
        const auto itemSize = elem["size"].As<long long>(0);

        trx.Execute(insertItem, id, parentId, uId, url,
                    uParent, type, itemSize, date);
        if (elem["type"].As<std::string>() == kFile) {
            trx.Execute(insertStory, uId, url, uParent, date);
            if (!parent.empty() && elem["type"].As<std::string>() == kFile) {
                updateParent(uParent.value(), itemSize, date, trx);
            }
        }
    }

    std::string getStringFromField(const storages::postgres::Field &elem) {
        if (!elem.IsNull())
            return boost::algorithm::trim_right_copy(elem.As<std::string>());
        else
            return {};
    }

    std::optional<formats::json::Value> getItemAndChildren(const boost::uuids::uuid &uid, const std::string &id,
                                                     storages::postgres::Transaction &trx){
        const std::string query = "WITH RECURSIVE r AS (\n"
                                  "   SELECT *, 1 AS level\n"
                                  "   FROM yet_another_disk.system_items\n"
                                  "   WHERE id = $1\n"
                                  "   UNION\n"
                                  "   SELECT items.*, r.level + 1 AS level\n"
                                  "   FROM yet_another_disk.system_items as items\n"
                                  "      JOIN r\n"
                                  "          ON items.parent_id = r.id\n"
                                  ")\n"
                                  "SELECT * FROM  r\n"
                                  "ORDER BY item_type DESC, level DESC;";
        auto res = trx.Execute(query, uid);
        if(!res.IsEmpty()){
            auto strJson = parseRes(res, id).dump(4);
            return formats::json::FromString(strJson);
        }
        else
            return {};
    }

    nlohmann::json parseRes (const storages::postgres::ResultSet &res, const std::string &resId){
        std::unordered_map<std::string, nlohmann::json> resultElems;
        std::vector<std::string> folderIds;
        for(const auto& elem: res){
            const auto elemId = getStringFromField(elem["id_string"]);
            const auto type = getStringFromField(elem["item_type"]);
            auto subRes = parseRow(elem);
            if(subRes.contains("parentId") && type == kFile){
                resultElems[subRes["parentId"]]["children"].push_back(subRes);
            }
            if(type == kFolder){
                folderIds.push_back(elemId);
            }
            resultElems[elemId] = std::move(subRes);
        }
        std::reverse(folderIds.begin(), folderIds.end());
        for(const auto& id: folderIds){
            const auto elemType = resultElems[id]["type"].get<std::string>();
            if(elemType == kFile)
                continue;
            if(!resultElems[id]["parentId"].is_null()){
                resultElems[resultElems[id]["parentId"].get<std::string>()]["children"].push_back(resultElems[id]);
            }
        }
        return resultElems[resId];
    }

    nlohmann::json parseRow(const storages::postgres::Row &row){
        const auto id = getStringFromField(row["id_string"]);
        const auto type = getStringFromField(row["item_type"]);
        const auto size = row["item_size"].As<long long>();
        const auto datePg = row["date-time"].As<storages::postgres::TimePointTz>();

        auto dateToParse = datePg.GetUnderlying();
        const auto date = utils::datetime::Timestring(dateToParse);

        std::optional<std::string> url;
        createOptional(row["url"], url);

        std::optional<std::string> parentId;
        createOptional(row["parent_string"], parentId);

        nlohmann::json jsonRes = {
                {"id", id},
                {"size", size},
                {"date", date},
                {"type", type}
        };


        auto addKey = [&jsonRes]<class T>(const std::string &key, std::optional<T> opt){
            if(opt.has_value())
                jsonRes[key] = opt.value();
            else
                jsonRes[key] = nullptr;
        };

        addKey("parentId", parentId);
        addKey("url", url);


        if (type == kFolder)
            jsonRes["children"] = nlohmann::json::array();
        else if(type == kFile)
            jsonRes["children"] = nullptr;

        return jsonRes;
    }

    template <typename T>
    void createOptional (const storages::postgres::Field &elem, std::optional<T> &res){
        if(elem.IsNull())
            res = std::nullopt;
        else if(std::is_same<T, std::string>())
            res = getStringFromField(elem);
        else
            res = elem.As<T>();
    }

    formats::json::Value notFound(const server::http::HttpRequest &request){
        request.SetResponseStatus(server::http::HttpStatus::kNotFound);
        return formats::json::MakeObject(
                "code", 404,
                "message", "Item not found"
        );
    }

    void AppendService(components::ComponentList &component_list) {
        component_list.Append<components::Postgres>("postgres-db-1");
        component_list.Append<clients::dns::Component>();
        component_list.Append<Imports>();
        component_list.Append<Nodes>();
        component_list.Append<Delete>();
    }

    void deleteElemById(const boost::uuids::uuid &id, storages::postgres::Transaction &trx) {
        const std::string query = "WITH RECURSIVE r AS (\n"
                                  "   SELECT *\n"
                                  "   FROM yet_another_disk.system_items\n"
                                  "   WHERE id = $1\n"
                                  "   UNION\n"
                                  "   SELECT items.*\n"
                                  "   FROM yet_another_disk.system_items as items\n"
                                  "      JOIN r\n"
                                  "          ON items.parent_id = r.id\n"
                                  ")\n"
                                  "DELETE\n"
                                  "FROM yet_another_disk.system_items\n"
                                  "WHERE id IN (SELECT id FROM r);";
        trx.Execute(query, id);
    }

}  // namespace yet_another_disk
