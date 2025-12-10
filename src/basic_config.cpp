#include "deribit/logging.h"
#include "deribit/websocket_beast.h"

int main () {
	// Setup logging
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	// Create WebSocket client
	deribit::WebSocketBeast ws;

	// Connect to Deribit WebSocket
	ws.connect();
	// Send a Deribit ping
	ws.send(R"({"jsonrpc":"2.0","id":1,"method":"public/ping"})");
	// Read pong
	const std::string resp = ws.read();
	LOG_INFO("Received: {}" + resp);
	// Close connection
	ws.close();
}