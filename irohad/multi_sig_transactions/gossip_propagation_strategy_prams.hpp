/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_GOSSIP_PROPAGATION_STRATEGY_PARAMS_HPP
#define IROHA_GOSSIP_PROPAGATION_STRATEGY_PARAMS_HPP

#include <chrono>

#include <boost/optional.hpp>

// TODO: IR-1317 @l4l (02/05/18) magics should be replaced with options via
// cli parameters
static const std::chrono::milliseconds kDefaultPeriod =
std::chrono::seconds(5);
static constexpr uint32_t kDefaultAmount = 2;

namespace iroha {
  /**
   * This structure provides configuration parameters for propagation strategy
   */
  struct GossipPropagationStrategyParams {
    /// period of emitting data in ms
    std::chrono::milliseconds period{kDefaultPeriod};

    /// amount of data (peers) emitted per once
    uint32_t amount{kDefaultAmount};
  };

}  // namespace iroha

#endif  // IROHA_GOSSIP_PROPAGATION_STRATEGY_PARAMS_HPP
