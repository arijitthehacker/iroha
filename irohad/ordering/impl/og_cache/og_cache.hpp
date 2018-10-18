/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_OG_CACHE_HPP
#define IROHA_OG_CACHE_HPP

#include "interfaces/iroha_internal/transaction_batch.hpp"

namespace iroha {
  namespace ordering {
    namespace cache {

      class OgCache {
       public:
        using BatchesListType = std::set<
            std::shared_ptr<shared_model::interface::TransactionBatch>>;

        virtual void addToBack(const BatchesListType &batches) = 0;

        virtual void up() = 0;

        virtual BatchesListType clearFrontAndGet() = 0;

        virtual void remove(const BatchesListType &batches) = 0;

        virtual const BatchesListType &front() const = 0;

        virtual const BatchesListType &back() const = 0;

        virtual ~OgCache() = default;
      };

    }  // namespace cache

  }  // namespace ordering
}  // namespace iroha

#endif  // IROHA_OG_CACHE_HPP