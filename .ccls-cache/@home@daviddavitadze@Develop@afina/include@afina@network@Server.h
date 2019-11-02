#ifndef AFINA_NETWORK_SERVER_H
#define AFINA_NETWORK_SERVER_H

#include <memory>
#include <vector>

namespace Afina {
class Storage;
namespace Logging {
class Service;
}
namespace Network {

/**
 * # Network processors coordinator
 * Configure resources for the network processors and coordinates all work
 */
class Server {
public:
    Server(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Afina::Logging::Service> pl)
        : pStorage(ps), pLogging(pl) {}
    virtual ~Server() {}

    /**
     * Starts network service. After method returns process should
     * listen on the given interface/port pair to process  incomming
     * data in workers number of threads
     */
    virtual void Start(uint16_t port, uint32_t acceptors = 1, uint32_t workers = 1) = 0;

    /**
     * Signal all worker threads that server is going to shutdown. After method returns
     * no more connections should be accepted, existing connections should stop receiving commands,
     * but must wait until currently runing commands are executed.
     *
     * After existing connections drain each should be closed and once worker has no more connection
     * its thread should be exited
     */
    virtual void Stop() = 0;

    /**
     * Blocks calling thread until all workers will be stopped and all resources allocated for the network
     * will be released
     */
    virtual void Join() = 0;

protected:
    /**
     * Instance of backing storage on which current server should execute
     * each command
     */
    std::shared_ptr<Afina::Storage> pStorage;

    /**
     * Logging service to be used in order to report application progress
     */
    std::shared_ptr<Afina::Logging::Service> pLogging;
};

} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_SERVER_H
