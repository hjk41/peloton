//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// insert_plan.h
//
// Identification: src/include/planner/insert_plan.h
//
// Copyright (c) 2015-17, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "planner/abstract_plan.h"
#include "planner/project_info.h"
#include "type/abstract_pool.h"

namespace peloton {

namespace storage {
class DataTable;
class Tuple;
}

namespace parser {
class InsertStatement;
}

namespace planner {
class InsertPlan : public AbstractPlan {
 public:
  // Construct when input is a logical tile
  InsertPlan(storage::DataTable *table, oid_t bulk_insert_count = 1)
    : target_table_(table), bulk_insert_count_(bulk_insert_count) {}

  // Construct with a project info
  InsertPlan(storage::DataTable *table,
             std::unique_ptr<const planner::ProjectInfo> &&project_info,
             oid_t bulk_insert_count = 1)
    : target_table_(table), project_info_(std::move(project_info)),
      bulk_insert_count_(bulk_insert_count) {
    LOG_TRACE("Creating an Insert Plan with a projection");
  }

  // Construct with a tuple
  InsertPlan(storage::DataTable *table,
             std::unique_ptr<storage::Tuple> &&tuple,
             oid_t bulk_insert_count = 1)
    : target_table_(table), bulk_insert_count_(bulk_insert_count) {
    LOG_TRACE("Creating an Insert Plan for one tuple");
    tuples_.push_back(std::move(tuple));
  }

  // Construct with specific values
  InsertPlan(storage::DataTable *table, const std::vector<std::string> *columns,
             const std::vector<std::vector<std::unique_ptr<
                 expression::AbstractExpression>>> *insert_values);

  // Get a varlen pool - will construct the pool only if needed
  type::AbstractPool *GetPlanPool();

  PlanNodeType GetPlanNodeType() const override { return PlanNodeType::INSERT; }

  void SetParameterValues(std::vector<type::Value> *values) override;

  storage::DataTable *GetTable() const { return target_table_; }

  const planner::ProjectInfo *GetProjectInfo() const {
    return project_info_.get();
  }

  oid_t GetBulkInsertCount() const { return bulk_insert_count_; }

  const storage::Tuple *GetTuple(int tuple_idx) const {
    if (tuple_idx >= (int)tuples_.size()) {
      return nullptr;
    }
    return tuples_[tuple_idx].get();
  }

  const std::string GetInfo() const override { return "InsertPlan"; }

  // WARNING - Not Implemented
  std::unique_ptr<AbstractPlan> Copy() const override {
    LOG_INFO("InsertPlan Copy() not implemented");
    std::unique_ptr<AbstractPlan> dummy;
    return dummy;
  }

 private:
  // Target table
  storage::DataTable *target_table_ = nullptr;

  // Projection Info
  std::unique_ptr<const planner::ProjectInfo> project_info_;

  // Tuple
  std::vector<std::unique_ptr<storage::Tuple>> tuples_;

  // Parameter Information <tuple_index, tuple_column_index, parameter_index>
  std::unique_ptr<std::vector<std::tuple<oid_t, oid_t, oid_t>>>
      parameter_vector_;

  // Parameter value types
  std::unique_ptr<std::vector<type::TypeId>> params_value_type_;

  // Number of times to insert
  oid_t bulk_insert_count_;

  // pool for variable length types
  std::unique_ptr<type::AbstractPool> pool_;

 private:
  DISALLOW_COPY_AND_MOVE(InsertPlan);
};

}  // namespace planner
}  // namespace peloton
