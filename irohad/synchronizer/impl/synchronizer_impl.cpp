/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utility>
#include "backend/protobuf/block.hpp"
#include "backend/protobuf/empty_block.hpp"
#include "interfaces/iroha_internal/block_variant.hpp"

#include "ametsuchi/mutable_storage.hpp"
#include "synchronizer/impl/synchronizer_impl.hpp"

namespace iroha {
  namespace synchronizer {

    SynchronizerImpl::SynchronizerImpl(
        std::shared_ptr<network::ConsensusGate> consensus_gate,
        std::shared_ptr<validation::ChainValidator> validator,
        std::shared_ptr<ametsuchi::MutableFactory> mutableFactory,
        std::shared_ptr<network::BlockLoader> blockLoader)
        : validator_(std::move(validator)),
          mutableFactory_(std::move(mutableFactory)),
          blockLoader_(std::move(blockLoader)),
          log_(logger::log("synchronizer")) {
      consensus_gate->on_commit().subscribe(
          subscription_,
          [&](const shared_model::interface::BlockVariant &block_variant) {
            this->process_commit(block_variant);
          });
    }

    SynchronizerImpl::~SynchronizerImpl() {
      subscription_.unsubscribe();
    }

    void SynchronizerImpl::process_commit(
        const shared_model::interface::BlockVariant &committed_block_variant) {
      log_->info("processing commit");
      auto storage = createTemporaryStorage();
      if (not storage) {
        return;
      }

      if (validator_->validateBlock(committed_block_variant, *storage)) {
        // block can be applied to current storage; do it and commit the result
        // to main ametsuchi, if it's not an empty block
        iroha::visit_in_place(
            committed_block_variant,
            [this, storage = std::move(storage)](
                std::shared_ptr<shared_model::interface::Block>
                    block_ptr) mutable {
              // we do not need a predicate, as we have already checked
              // applicability of the block
              storage->apply(
                  *block_ptr,
                  [](const auto &, auto &, const auto &) { return true; });
              mutableFactory_->commit(std::move(storage));

              notifier_.get_subscriber().on_next(
                  rxcpp::observable<>::just(block_ptr));
            },
            [this](std::shared_ptr<shared_model::interface::EmptyBlock>
                       empty_block_ptr) {
              // we have an empty block, so don't apply it, just notify the
              // subscriber
              notifier_.get_subscriber().on_next(
                  rxcpp::observable<>::just(empty_block_ptr));
            });
      } else {
        // block can't be applied to current storage
        // download all missing blocks
        for (const auto &signature : committed_block_variant.signatures()) {
          storage = createTemporaryStorage();
          if (not storage) {
            return;
          }

          auto network_chain = blockLoader_->retrieveBlocks(
              shared_model::crypto::PublicKey(signature.publicKey()));
          std::vector<std::shared_ptr<shared_model::interface::Block>> blocks;
          network_chain.as_blocking().subscribe(
              [&blocks](auto block) { blocks.push_back(block); });
          auto chain_ends_with_right_block =
              blocks.back()->hash() == committed_block_variant.prevHash();
          auto chain =
              rxcpp::observable<>::iterate(blocks, rxcpp::identity_immediate());

          if (validator_->validateChain(chain, *storage)
              and chain_ends_with_right_block) {
            // peer sent valid chain; apply our last block, if it's not empty,
            // and commit it to ametsuchi
            notifier_.get_subscriber().on_next(chain);

            storage = iroha::visit_in_place(
                committed_block_variant,
                [this, storage = std::move(storage), chain = std::move(chain)](
                    std::shared_ptr<shared_model::interface::Block>
                        block_ptr) mutable {
                  // if last block is not empty, update storage and notifier
                  // with it
                  notifier_.get_subscriber().on_next(
                      rxcpp::observable<>::just(block_ptr));
                  storage->apply(
                      *block_ptr,
                      [](const auto &, auto &, const auto &) { return true; });
                  return std::move(storage);
                },
                [storage = std::move(storage)]() mutable {
                  return std::move(storage);
                });
            mutableFactory_->commit(std::move(storage));
            // we are synchronized
            return;
          }
        }
      }
    }

    rxcpp::observable<Commit> SynchronizerImpl::on_commit_chain() {
      return notifier_.get_observable();
    }

    std::unique_ptr<ametsuchi::MutableStorage>
    SynchronizerImpl::createTemporaryStorage() const {
      return mutableFactory_->createMutableStorage().match(
          [](expected::Value<std::unique_ptr<ametsuchi::MutableStorage>>
                 &created_storage) { return std::move(created_storage.value); },
          [this](expected::Error<std::string> &error) {
            log_->error("could not create mutable storage: {}", error.error);
            return nullptr;
          });
    }

  }  // namespace synchronizer
}  // namespace iroha
