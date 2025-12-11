#include "deribit/logging.h"
#include "deribit/websocket_beast.h"

/**
 * Minimal example program that demonstrates basic setup and usage.
 *
 * The program initializes logging, connects to the Deribit testnet WebSocket,
 * sends a simple public ping RPC, reads the response and logs it, then closes
 * the connection. It serves as a small smoke test for the WebSocket and
 * logging helpers in the repository.
 */
int main() {
	/**
	 * Initialize library logging and enable debug verbosity for this demo.
	 */
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::WebSocketBeast ws;

	ws.connect();

	ws.send(R"({"jsonrpc":"2.0","id":1,"method":"public/ping"})");

	auto resp = ws.read();

	LOG_INFO("Received: {}", resp);

	ws.close();
}
