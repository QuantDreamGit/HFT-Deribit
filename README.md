# Deribit HFT Client

This project provides a high-performance client for interacting with the **Deribit API** via WebSockets. It supports subscribing to public channels and sending RPC (Remote Procedure Call) requests, while ensuring rate limiting to comply with API usage restrictions. The client uses background worker threads for reading and sending messages, and it is designed for high-frequency trading (HFT) applications.

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Getting Started](#getting-started)
4. [Configuration](#configuration)
5. [Usage](#usage)
6. [Rate Limiting](#rate-limiting)
7. [Authentication](#authentication)
8. [Contributing](#contributing)
9. [License](#license)

## Overview

The **Deribit HFT Client** is a simple yet powerful client that communicates with the Deribit WebSocket API for both public and private channels. It is designed to be flexible, efficient, and easy to extend. The client supports:

* Subscribing to public channels (e.g., market data like BTC price).
* Sending custom RPC requests to interact with the API.
* Automatically handling messages through a dispatcher.
* Using a **rate limiter** to prevent exceeding API request limits.

## Features

* **WebSocket Connection**: Establishes a secure WebSocket connection to the Deribit API.
* **Subscriptions**: Allows subscribing to various public channels, such as market data (e.g., "deribit_price_index.btc_usd").
* **RPC Requests**: Enables sending RPC requests to the Deribit API, such as `public/ping` or `private/subscribe`.
* **Rate Limiting**: Controls the rate at which requests are sent to avoid hitting API rate limits.
* **Background Workers**: Uses background threads for both receiving and sending messages efficiently.

## Getting Started

To get started, you need to clone this repository and install the necessary dependencies. Here's how you can set up the project:

### Prerequisites

* **C++20** or later.
* **Boost** library (for WebSocket and Asio support).
* **spdlog** library (for logging).
* **simdjson** (for efficient JSON parsing).

### Clone the Repository

```bash
git clone https://github.com/your-username/deribit-hft-client.git
cd deribit-hft-client
```

### Install Dependencies

If you're using **CMake**, you can install the necessary dependencies using the following command:

```bash
mkdir build
cd build
cmake ..
make
```

## Configuration

Before running the client, you need to provide the necessary credentials for authentication. You can either:

1. **Set environment variables** for the `client_id` and `client_secret`, or
2. Modify the code directly to pass these credentials.

### Environment Variables

Set the following environment variables for the client to authenticate:

```bash
export DERIBIT_CLIENT_ID="your_client_id"
export DERIBIT_CLIENT_SECRET="your_client_secret"
```

Alternatively, you can define these variables in a `.env` file for automatic loading at runtime.

### Authentication

The client supports **OAuth2 client credentials flow** for authentication. The access token is fetched when the client connects to Deribit.

**Note**: Authentication happens automatically when the client starts if valid credentials are provided. If you need more granular control over authentication, you can manually request a token.

## Usage

Here’s an example of how to use the client to subscribe to a channel and process incoming market data.

```cpp
#include "deribit_client.h"

int main() {
    // Initialize logging
    deribit::init_logging();
    SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

    // Create and connect the client
    deribit::DeribitClient client;
    client.load_credentials_from_env();
    client.connect();

    // Register a subscription callback for the BTC index price channel
    client.register_subscription("deribit_price_index.btc_usd", [](const deribit::ParsedMessage& pm) {
        LOG_INFO("Subscription received");
        LOG_INFO("Channel = {}", pm.channel);
        LOG_INFO("Data = {}", pm.data);
    });

    // Subscribe to the BTC index price channel
    client.subscribe("deribit_price_index.btc_usd");

    // Poll for incoming messages and handle them
    while (true) {
        client.poll();
    }

    // Close the client when done
    client.close();
    return 0;
}
```

### Key Methods

* **`load_credentials_from_env()`**: Loads the `client_id` and `client_secret` from environment variables.
* **`connect()`**: Establishes a WebSocket connection and starts background threads for sending and receiving messages.
* **`register_subscription(channel, callback)`**: Registers a callback for a specific subscription channel (e.g., market data).
* **`subscribe(channel)`**: Subscribes to a public channel (e.g., `deribit_price_index.btc_usd`).
* **`send_rpc(id, method, params)`**: Sends a custom RPC request.
* **`poll()`**: Polls the inbound queue for messages and dispatches them to the appropriate handlers.

## Rate Limiting

The client uses a **rate limiter** to ensure that the number of requests sent does not exceed the limits imposed by Deribit’s API.

* The rate limiter is based on **tokens**.
* Each request consumes 1 token.
* Tokens are replenished over time at a fixed rate (e.g., 5 tokens per second).
* If a request is made when no tokens are available, it will be denied.

**Rate limiting is applied** in the `subscribe()` and `send_rpc()` methods to prevent overwhelming the Deribit server with requests.

### RateLimiter Example:

```cpp
deribit::RateLimiter rate_limiter;
if (rate_limiter.allow_request()) {
    // Send the request
} else {
    // Log warning: Rate limit exceeded
}
```

## Contributing

We welcome contributions! If you have suggestions for improvements, please open an issue or submit a pull request. Here are some ways you can help:

* Fix bugs or improve existing functionality.
* Add new features or methods to the client.
* Improve the documentation or provide examples.
* Help with testing.

Please ensure that your code follows the project's coding style and passes all the existing tests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
