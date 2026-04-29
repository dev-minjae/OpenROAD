// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <vector>

namespace odb {
class dbBlock;
class dbDatabase;
class dbInst;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// iPL-3D paper §IV.B parameters (Table III defaults).
struct TierOptParams
{
  double rho = 500.0;      // terminal cost
  double alpha = 100.0;    // overflow cost
  double beta = 0.5;       // overlap cost
  double gamma = 0.0;      // 0 normally, 1e4 for high-density relief
  double B_factor = 1.1;   // knapsack: B = B_factor * u_t * A
  int max_outer_iter = 1;  // VNS outer iterations (Phase 4.3 uses >1)
};

// Algorithm 2 from paper §IV.B-2/3. Phase 4.1: skeleton only — run()
// returns an empty vector and logs an info message. Phase 4.2 implements
// the priority-queue + surrogate body.
class GlobalTierOptimizer
{
 public:
  GlobalTierOptimizer(odb::dbDatabase* db, utl::Logger* logger);

  // Given a flattened placement (all cells in `from_block`), returns the
  // cells whose partition_id should flip toward `to_block`. Caller is
  // responsible for actually applying the partition decision.
  std::vector<odb::dbInst*> run(odb::dbBlock* from_block,
                                odb::dbBlock* to_block,
                                const TierOptParams& params);

 private:
  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
};

}  // namespace mdm
