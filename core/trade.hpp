#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace murim {

/// A ware people buy and consume, as data: its `name`, its `price` in
/// silver, the `health` it restores, up to `newborn_health`, and the `qi` it
/// restores, up to the consumer's store. It is sold by people whose
/// occupation sells it (ADR 0081).
struct Ware {
  std::string name;
  std::uint64_t price = 0;
  std::uint64_t health = 0;
  std::uint32_t qi = 0;
};

/// How silver and wares go round, as data, one rule for every person. Each
/// person is born holding `purse` silver and none of the `wares`, and once a
/// day every living person with a home earns `wage` silver. A buyer's silver
/// goes to the seller. Every person no driver steers, once a day, buys one
/// of each ware it holds none of from the first person it may talk to who
/// sells it, while it has the price; and each tick consumes a ware that
/// restores health while its health is at most `wounded` per mille of
/// `newborn_health`, and one that restores qi while its qi is at most
/// `drained` per mille of its store.
struct Trade {
  std::vector<Ware> wares;
  std::uint64_t purse = 0;
  std::uint64_t wage = 0;
  std::uint32_t wounded = 0;
  std::uint32_t drained = 0;
};

/// Why a person may not buy a ware from another, the first that holds in
/// this order; `NONE` when it may.
enum class BuyRefusal : std::uint8_t {
  NONE,
  /// Either person is dead.
  DEAD,
  /// The buyer may not talk to the seller, as talk_refusal() says.
  CANNOT_TALK,
  /// The seller does not sell the ware, as sells() says.
  NOT_SOLD,
  /// The buyer holds less silver than the ware's price.
  NO_SILVER,
};

/// Why a person may not consume a ware, the first that holds in this order;
/// `NONE` when it may.
enum class ConsumeRefusal : std::uint8_t {
  NONE,
  /// The person is dead.
  DEAD,
  /// The person holds none of the ware.
  NONE_HELD,
  /// Nothing the ware restores would rise: its health is at newborn_health
  /// or the ware restores none, and its qi is at its store or the ware
  /// restores none.
  FULL,
};

/// Throws std::invalid_argument when `trade` lies outside the limits the
/// constructor documents.
void check_trade(const Trade& trade);

}  // namespace murim
