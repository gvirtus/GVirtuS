#pragma once

#include <cstddef>
#include <memory>
#include "Endpoint.h"

namespace gvirtus::communicators {
/**
 * Communicator is an abstract class that implements a simple stream oriented
 * mechanism for communicating with two end points.
 * Communicator use a client/server approach, for having a Communicator server
 * the application must call Serve() and the Accept() for accepting the
 * connection by clients and communicating to them.
 * The client has to use just the Connect() method.
 * For sending and receiving data through the communicator is possible the use
 * the input and output stream. Warning: _never_ try to communicate through the
 * streams of a server Communicator, for communicating with the client the
 * Communicator returned from the Accept() must be used.
 */
class Communicator {
 public:
  /**
   * Creates a new communicator. The real type of the communicator and his
   * parameters are obtained from the ConfigFile::Element @arg config.
   *
   * @param config the ConfigFile::Element that stores the configuration.
   *
   * @return a new Communicator.
   */

  virtual ~Communicator() = default;

  /**
   * Sets the communicator as a server.
   */
  virtual void Serve() = 0;

  /**
   * Accepts a new connection. The call to the first Accept() must follow a
   * call to Serve().
   *
   * @return a Communicator to the connected peer.
   */
  virtual const Communicator *Accept() const = 0;

  /**
   * Sets the communicator as a client and connects it to the end point
   * specified in the ConfigFile::Element used to build this Communicator.
   */
  virtual void Connect() = 0;

  virtual size_t Read(char *buffer, size_t size) = 0;
  virtual size_t Write(const char *buffer, size_t size) = 0;
  virtual void Sync() = 0;

  /**
   * Closes the connection with the end point.
   */
  virtual void Close() = 0;

  virtual std::string to_string() { return "communicator"; }

  virtual void run(){};

 private:
};

using create_t = std::shared_ptr<Communicator>(std::shared_ptr<Endpoint>);

}  // namespace gvirtus::communicators
