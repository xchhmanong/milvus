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

#include "server/delivery/request/CountTableRequest.h"
#include "BaseRequest.h"
#include "server/DBWrapper.h"
#include "utils/Log.h"
#include "utils/TimeRecorder.h"
#include "utils/ValidationUtil.h"

#include <fiu-local.h>
#include <memory>

namespace milvus {
namespace server {

CountTableRequest::CountTableRequest(const std::shared_ptr<milvus::server::Context>& context,
                                     const std::string& table_name, int64_t& row_count)
    : BaseRequest(context, BaseRequest::kCountTable), table_name_(table_name), row_count_(row_count) {
}

BaseRequestPtr
CountTableRequest::Create(const std::shared_ptr<milvus::server::Context>& context, const std::string& table_name,
                          int64_t& row_count) {
    return std::shared_ptr<BaseRequest>(new CountTableRequest(context, table_name, row_count));
}

Status
CountTableRequest::OnExecute() {
    try {
        std::string hdr = "CountTableRequest(table=" + table_name_ + ")";
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

        // step 2: get row count
        uint64_t row_count = 0;
        status = DBWrapper::DB()->GetTableRowCount(table_name_, row_count);
        fiu_do_on("CountTableRequest.OnExecute.db_not_found", status = Status(DB_NOT_FOUND, ""));
        fiu_do_on("CountTableRequest.OnExecute.status_error", status = Status(SERVER_UNEXPECTED_ERROR, ""));
        fiu_do_on("CountTableRequest.OnExecute.throw_std_exception", throw std::exception());
        if (!status.ok()) {
            if (status.code() == DB_NOT_FOUND) {
                return Status(SERVER_TABLE_NOT_EXIST, TableNotExistMsg(table_name_));
            } else {
                return status;
            }
        }

        row_count_ = static_cast<int64_t>(row_count);
    } catch (std::exception& ex) {
        return Status(SERVER_UNEXPECTED_ERROR, ex.what());
    }

    return Status::OK();
}

}  // namespace server
}  // namespace milvus
