#include "Processor.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

namespace {

std::string firstInsert(const juce::var& state, const char* lane) {
  if (const auto* items = state[lane].getArray())
    for (const auto& item : *items)
      if (item["kind"].toString() == "insert")
        return item["blockId"].toString().toStdString();
  return {};
}

TEST(ReverbBlock, PersistsAndRestoresWithoutModelBytes) {
  ChainTestProcessor proc;
  proc.setPlayConfigDetails(2, 2, kFs, 512);
  proc.prepareToPlay(kFs, 512);
  const auto id = proc.addReverbBlock(firstInsert(proc.getChainState(-1), "chain"));
  ASSERT_FALSE(id.empty());
  ASSERT_TRUE(proc.setBlockParam(id, "mix", 0.37));
  ASSERT_TRUE(proc.setBlockParam(id, "roomSize", 0.82));
  const auto state = proc.getChainState(-1);
  EXPECT_EQ((*state["chain"].getArray())[0]["kind"].toString(), "reverb");

  juce::MemoryBlock bytes;
  proc.getStateInformation(bytes);
  ChainTestProcessor restored;
  restored.setPlayConfigDetails(2, 2, kFs, 512);
  restored.prepareToPlay(kFs, 512);
  restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
  const auto restoredState = restored.getChainState(-1);
  const auto item = (*restoredState["chain"].getArray())[0];
  EXPECT_EQ(item["kind"].toString(), "reverb");
  EXPECT_NEAR(static_cast<double>(item["params"]["mix"]), 0.37, 1e-5);
  EXPECT_NEAR(static_cast<double>(item["params"]["roomSize"]), 0.82, 1e-5);
  EXPECT_TRUE(static_cast<bool>(item["loaded"]));
}

TEST(ReverbBlock, UndoRestoresRoomSizeAndCopyCanMoveToOtherLane) {
  ChainTestProcessor proc;
  proc.setPlayConfigDetails(2, 2, kFs, 512);
  proc.prepareToPlay(kFs, 512);
  const auto id = proc.addReverbBlock(firstInsert(proc.getChainState(-1), "chain"));
  ASSERT_FALSE(id.empty());
  ASSERT_TRUE(proc.setBlockParam(id, "roomSize", 0.8));
  ASSERT_TRUE(proc.undoChain());
  const auto restored = (*proc.getChainState(-1)["chain"].getArray())[0];
  EXPECT_NEAR(static_cast<double>(restored["params"]["roomSize"]), 0.5, 1e-5);

  proc.setStereoMode(true);
  ASSERT_TRUE(proc.copyChainBlock(id));
  const auto pasted = proc.pasteChainBlock("right", 0);
  ASSERT_FALSE(pasted.empty());
  const auto right = (*proc.getChainState(-1)["chainRight"].getArray())[0];
  EXPECT_EQ(right["kind"].toString(), "reverb");
  EXPECT_TRUE(static_cast<bool>(right["loaded"]));
}

TEST(ReverbBlock, BranchProcessesOnlyTheLeftOutput) {
  ChainTestProcessor proc;
  proc.setPlayConfigDetails(2, 2, kFs, 512);
  proc.prepareToPlay(kFs, 512);
  seedStereoChains(proc, {"amp"}, {});
  ASSERT_TRUE(waitForChainLoaded(proc));
  const auto id = proc.addReverbBlock(firstInsert(proc.getChainState(-1), "chain"));
  ASSERT_FALSE(id.empty());
  ASSERT_TRUE(proc.setChainBranch("left", "amp"));
  ASSERT_TRUE(proc.setBlockParam(id, "mix", 1.0));
  ASSERT_TRUE(proc.setBlockParam(id, "roomSize", 0.8));

  const auto input = makeNoise(240 * 512, 12345, 0.1f);
  const auto [wetL, dryR] = processStereo(proc, input);
  EXPECT_GT(settledMaxChannelDiff(wetL, dryR), 0.001f);

  ASSERT_TRUE(proc.setBlockParam(id, "enabled", 0.0));
  const auto [bypassL, bypassR] = processStereo(proc, input);
  EXPECT_LT(settledMaxChannelDiff(bypassL, bypassR), 1e-4f);
}

}  // namespace
