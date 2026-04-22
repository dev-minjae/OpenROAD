// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "mdm/MultiDieManager.h"
#include "odb/db.h"

namespace mdm {

using std::pair;
using std::string;
using std::vector;

namespace {

int findAssignedDieId(odb::dbInst* inst)
{
  auto* partition_info = odb::dbIntProperty::find(inst, "partition_id");
  if (!partition_info) {
    return 0;  // default to die 0 if no partition info
  }
  return partition_info->getValue();
}

pair<odb::dbBlock*, odb::dbLib*> findTargetDieAndLib(MultiDieManager* manager,
                                                     int die_id)
{
  odb::dbBlock* target_block = nullptr;
  odb::dbLib* target_lib = nullptr;

  auto block_iter
      = manager->getDb()->getChip()->getBlock()->getChildren().begin();
  for (int i = 0; i < die_id; ++i) {
    ++block_iter;
  }
  target_block = *block_iter;

  auto lib_iter = manager->getDb()->getLibs().begin();
  ++lib_iter;  // skip the TopHierLib
  for (int i = 0; i < die_id; ++i) {
    ++lib_iter;
  }
  target_lib = *lib_iter;
  return {target_block, target_lib};
}

void inheritPlacementInfo(odb::dbInst* original_inst, odb::dbInst* new_inst)
{
  const auto status = original_inst->getPlacementStatus();
  if (status == odb::dbPlacementStatus::FIRM) {
    new_inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
  } else {
    new_inst->setPlacementStatus(status);
  }
  if (status != odb::dbPlacementStatus::NONE) {
    const odb::Point loc = original_inst->getLocation();
    new_inst->setLocation(loc.x(), loc.y());
  }
}

void inheritNetInfo(odb::dbInst* original_inst, odb::dbInst* new_inst)
{
  vector<pair<string, odb::dbNet*>> net_info_container;
  odb::dbBlock* target_block = new_inst->getBlock();
  for (auto iterm : original_inst->getITerms()) {
    if (!iterm->getNet()) {
      continue;
    }
    net_info_container.emplace_back(iterm->getMTerm()->getName(),
                                    iterm->getNet());
  }
  for (const auto& info : net_info_container) {
    odb::dbITerm* iterm = new_inst->findITerm(info.first.c_str());
    assert(iterm != nullptr);
    odb::dbNet* original_net = info.second;
    odb::dbNet* new_net
        = target_block->findNet(original_net->getName().c_str());
    if (!new_net) {
      new_net
          = odb::dbNet::create(target_block, original_net->getName().c_str());
      assert(new_net);
    }
    iterm->connect(new_net);
  }
}

void inheritProperties(odb::dbInst* original_inst, odb::dbInst* new_inst)
{
  struct PropertyStorage
  {
    vector<int> ints;
    vector<string> strings;
    vector<double> doubles;
    vector<char> bools;  // vector<bool> is a bitset; use char for safety
    vector<string> int_names;
    vector<string> string_names;
    vector<string> double_names;
    vector<string> bool_names;
  };
  PropertyStorage storage;
  for (auto* property : odb::dbProperty::getProperties(original_inst)) {
    const auto name = property->getName();
    switch (property->getType()) {
      case odb::dbProperty::INT_PROP:
        storage.ints.push_back(
            odb::dbIntProperty::find(original_inst, name.c_str())->getValue());
        storage.int_names.push_back(name);
        break;
      case odb::dbProperty::STRING_PROP:
        storage.strings.push_back(
            odb::dbStringProperty::find(original_inst, name.c_str())
                ->getValue());
        storage.string_names.push_back(name);
        break;
      case odb::dbProperty::DOUBLE_PROP:
        storage.doubles.push_back(
            odb::dbDoubleProperty::find(original_inst, name.c_str())
                ->getValue());
        storage.double_names.push_back(name);
        break;
      case odb::dbProperty::BOOL_PROP:
        storage.bools.push_back(
            odb::dbBoolProperty::find(original_inst, name.c_str())->getValue()
                ? 1
                : 0);
        storage.bool_names.push_back(name);
        break;
    }
  }
  for (size_t i = 0; i < storage.ints.size(); ++i) {
    odb::dbIntProperty::create(
        new_inst, storage.int_names[i].c_str(), storage.ints[i]);
  }
  for (size_t i = 0; i < storage.strings.size(); ++i) {
    odb::dbStringProperty::create(
        new_inst, storage.string_names[i].c_str(), storage.strings[i].c_str());
  }
  for (size_t i = 0; i < storage.doubles.size(); ++i) {
    odb::dbDoubleProperty::create(
        new_inst, storage.double_names[i].c_str(), storage.doubles[i]);
  }
  for (size_t i = 0; i < storage.bools.size(); ++i) {
    odb::dbBoolProperty::create(
        new_inst, storage.bool_names[i].c_str(), storage.bools[i] != 0);
  }
}

void inheritGroupInfo(odb::dbInst* original_inst, odb::dbInst* new_inst)
{
  odb::dbGroup* group = original_inst->getGroup();
  if (!group) {
    return;
  }
  group->addInst(new_inst);
}

}  // namespace

void SwitchInstanceHelper::switchInstanceToAssignedDie(
    MultiDieManager* manager,
    odb::dbInst* original_inst)
{
  const int die_id = findAssignedDieId(original_inst);
  auto [target_block, target_lib] = findTargetDieAndLib(manager, die_id);
  odb::dbMaster* target_master
      = target_lib->findMaster(original_inst->getMaster()->getName().c_str());
  assert(target_master != nullptr);

  if (!target_master->getSite()) {
    // The child block's site is set up by TestCaseManager::rowConstruction,
    // so the row iterator must be non-empty. Setting the master's site lets
    // downstream placers query a valid site for the master.
    target_master->setSite((*target_block->getRows().begin())->getSite());
  }

  odb::dbInst* new_inst = odb::dbInst::create(
      target_block, target_master, original_inst->getName().c_str());
  inheritPlacementInfo(original_inst, new_inst);
  inheritNetInfo(original_inst, new_inst);
  inheritProperties(original_inst, new_inst);
  inheritGroupInfo(original_inst, new_inst);

  odb::dbInst::destroy(original_inst);
}

}  // namespace mdm
