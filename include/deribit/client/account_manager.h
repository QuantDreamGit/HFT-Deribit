#ifndef HFTDERIBIT_ACCOUNT_H
#define HFTDERIBIT_ACCOUNT_H

#include <mutex>
#include <string>
#include <unordered_map>

namespace deribit {

	class DeribitClient;        // forward declaration
	struct ParsedMessage;       // forward declaration

	struct CurrencyAccountState {
		double equity{};
		double balance{};
		double available_funds{};
		double margin_balance{};
		double initial_margin{};
		double maintenance_margin{};
		double unrealized_pnl{};
		double realized_pnl{};
		double total_delta{};
		double liquidation_ratio{};
	};

	class AccountManager {
	public:
		AccountManager() = default;

		void initialize(DeribitClient& client);
		void initialize_from_snapshot(const std::string& json_snapshot);

		CurrencyAccountState get_state(const std::string& currency) const;

	private:
		mutable std::mutex mtx_;
		std::unordered_map<std::string, CurrencyAccountState> accounts_;
	};

} // namespace deribit

#endif
