// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

#include "server/delivery/request/DropIndexRequest.h"
#include "server/DBWrapper.h"
#include "utils/Log.h"
#include "utils/TimeRecorder.h"
#include "utils/ValidationUtil.h"

#include <fiu-local.h>
#include <memory>

namespace milvus {
namespace server {

DropIndexRequest::DropIndexRequest(const std::shared_ptr<milvus::server::Context>& context,
                                   const std::string& table_name)
    : BaseRequest(context, BaseRequest::kDropIndex), table_name_(table_name) {
}

BaseRequestPtr
DropIndexRequest::Create(const std::shared_ptr<milvus::server::Context>& context, const std::string& table_name) {
    return std::shared_ptr<BaseRequest>(new DropIndexRequest(context, table_name));
}

Status
DropIndexRequest::OnExecute() {
    try {
        fiu_do_on("DropIndexRequest.OnExecute.throw_std_exception", throw std::exception());
        std::string hdr = "DropIndexRequest(table=" + table_name_ + ")";
        TimeRecorderAuto rc(hdr);

        // step 1: check arguments
        auto status = ValidationUtil::ValidateTableName(table_name_);
        if (!status.ok()) {
            return status;
        }

        // only process root table, ignore partition table
        engine::meta::TableSchema table_schema;
        table_schema.table_id_ = table_name_;
        status = DBWrapper::DB()->DescribeTable(table_schema);
        fiu_do_on("DropIndexRequest.OnExecute.table_not_exist", status = Status(milvus::SERVER_UNEXPECTED_ERROR, ""));
        if (!status.ok()) {
            if (status.code() == DB_NOT_FOUND) {
                return Status(SERVER_TABLE_NOT_EXIST, TableNotExistMsg(table_name_));
            } else {
                return status;
            }
        } else {
            if (!table_schema.owner_table_.empty()) {
                return Status(SERVER_INVALID_TABLE_NAME, TableNotExistMsg(table_name_));
            }
        }

        rc.RecordSection("check validation");

        // step 2: drop index
        status = DBWrapper::DB()->DropIndex(table_name_);
        fiu_do_on("DropIndexRequest.OnExecute.drop_index_fail", status = Status(milvus::SERVER_UNEXPECTED_ERROR, ""));
        if (!status.ok()) {
            return status;
        }
    } catch (std::exception& ex) {
        return Status(SERVER_UNEXPECTED_ERROR, ex.what());
    }

    return Status::OK();
}

}  // namespace server
}  // namespace milvus
