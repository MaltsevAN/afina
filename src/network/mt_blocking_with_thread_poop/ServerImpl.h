//
// Created by alexmal on 17.10.18.
//

#ifndef AFINA_SERVERIMPL_H
#define AFINA_SERVERIMPL_H


#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

#include <afina/network/Server.h>
#include <afina/Executor.h>

namespace spdlog {
    class logger;
}

namespace Afina {
    namespace Network {
        namespace MT_thread_pool {

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
            class ServerImpl : public Server {
            public:
                ServerImpl(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Logging::Service> pl);
                ~ServerImpl();

                // See Server.h
                void Start(uint16_t port, uint32_t, uint32_t) override;

                // See Server.h
                void Stop() override;

                // See Server.h
                void Join() override;

            protected:
                /**
                 * Method is running in the connection acceptor thread
                 */
                void OnRun();

            private:
                // Logger instance
                std::shared_ptr<spdlog::logger> _logger;

                // Atomic flag to notify threads when it is time to stop. Note that
                // flag must be atomic in order to safely publisj changes cross thread
                // bounds
                std::atomic<bool> running;

                // Server socket to accept connections on
                int _server_socket;

                // Thread to run network on
                std::thread _thread;
                void _Worker(int client_socket);
                Executor _executor;
            };

        } // namespace MT_thread_pool
    } // namespace Network
} // namespace Afina

#endif //AFINA_SERVERIMPL_H
