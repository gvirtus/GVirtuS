/*
 * gVirtuS -- A GPGPU transparent virtualization component.
 *
 * Copyright (C) 2009-2010  The University of Napoli Parthenope at Naples.
 *
 * This file is part of gVirtuS.
 *
 * gVirtuS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gVirtuS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gVirtuS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by: Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>,
 *             Department of Applied Science
 */

/**
 * @file   TcpCommunicator.cpp
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Thu Oct 8 12:08:33 2009
 *
 * @brief
 *
 *
 */
//#define DEBUG

#include "TcpCommunicator.h"

#ifndef _WIN32

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#else
#include <WinSock2.h>
static bool initialized = false;
#endif

#ifndef GVIRTUS_TCP_NO_PLUGIN_FACTORY
#include <gvirtus/communicators/Endpoint.h>
#include <gvirtus/communicators/Endpoint_Tcp.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace std;
using gvirtus::communicators::TcpCommunicator;

namespace {
#ifndef _WIN32
void close_socket(int &fd) {
    if (fd < 0) return;
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    fd = -1;
}

void configure_socket(int fd) {
    const int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
}
#else
void configure_socket(int fd) {
    const char enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
}
#endif
}  // namespace

TcpCommunicator::TcpCommunicator(const std::string &communicator) {
#ifdef _WIN32
    if (!initialized) {
      WSADATA data;
      if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        throw "Cannot initialized WinSock.";
      initialized = true;
    }
#endif

    const char *separator = strstr(communicator.c_str(), "://");
    if (separator == nullptr)
        throw std::string("TcpCommunicator: Invalid endpoint '") + communicator + "'.";
    const char *valueptr = separator + 3;
    const char *portptr = strchr(valueptr, ':');
    if (portptr == NULL)
        throw "Port not specified.";
    mPort = (short) strtol(portptr + 1, NULL, 10);

#ifdef _WIN32
    char *hostname = _strdup(valueptr);
#else
    char *hostname = strdup(valueptr);
#endif

    hostname[portptr - valueptr] = 0;
    mHostname = string(hostname);
    struct hostent *ent = gethostbyname(hostname);
    free(hostname);
    if (ent == NULL)
        throw "TcpCommunicator: Can't resolve hostname '" + mHostname + "'.";
    mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
}

TcpCommunicator::TcpCommunicator(const char *hostname, short port) {
    mHostname = string(hostname);
    struct hostent *ent = gethostbyname(hostname);
    if (ent == NULL)
        throw "TcpCommunicator: Can't resolve hostname '" + mHostname + "'.";
    mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
    mPort = port;
}

TcpCommunicator::TcpCommunicator(int fd, const char *hostname) {
    mSocketFd = fd;
    mHostname = hostname == nullptr ? std::string() : std::string(hostname);
    configure_socket(mSocketFd);
}

TcpCommunicator::~TcpCommunicator() {
    Close();
    delete[] mInAddr;
}

void TcpCommunicator::Serve() {
#ifdef DEBUG
    printf("TcpCommunicator::Serve() called\n");
#endif

    struct sockaddr_in socket_addr;

    if ((mSocketFd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw "TcpCommunicator: Can't create socket: " + std::string(strerror(errno)) + ".";

    memset((char *) &socket_addr, 0, sizeof(struct sockaddr_in));

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(mPort);
    socket_addr.sin_addr.s_addr = INADDR_ANY;

    int on = 1;
    setsockopt(mSocketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    configure_socket(mSocketFd);

    int bindResult = ::bind(mSocketFd, (struct sockaddr *) &socket_addr, sizeof(struct sockaddr_in));
    if (bindResult != 0)
        throw "TcpCommunicator: Can't bind socket: " + std::string(strerror(errno)) + ".";

    int listenResult = ::listen(mSocketFd, 5);
    if (listenResult != 0)
        throw "TcpCommunicator: Can't listen from socket: " + std::string(strerror(errno)) + ".";

#ifdef DEBUG
    printf("TcpCommunicator::Serve() returned\n");
#endif
}

const gvirtus::communicators::Communicator *TcpCommunicator::Accept() const {
#ifdef DEBUG
    printf("TcpCommunicator::Accept() called\n");
#endif

    int client_socket_fd;
    struct sockaddr_in client_socket_addr;
#ifndef _WIN32
    socklen_t client_socket_addr_size;
#else
    int client_socket_addr_size;
#endif

    client_socket_addr_size = sizeof(struct sockaddr_in);
    do {
        client_socket_fd = ::accept(mSocketFd, (sockaddr *)&client_socket_addr,
                                  &client_socket_addr_size);
    } while (client_socket_fd < 0 && errno == EINTR);
    if (client_socket_fd < 0)
        throw "TcpCommunicator: Can't accept connection: " +
              std::string(strerror(errno)) + ".";

#ifdef DEBUG
    printf("TcpCommunicator::Accept() returned\n");
#endif
    return new TcpCommunicator(client_socket_fd, inet_ntoa(client_socket_addr.sin_addr));
}

void TcpCommunicator::Connect() {
#ifdef DEBUG
    printf("TcpCommunicator::Connect() called\n");
#endif

    struct sockaddr_in remote;

    if ((mSocketFd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw "TcpCommunicator: Can't create socket: " + std::string(strerror(errno)) + ".";

    remote.sin_family = AF_INET;
    remote.sin_port = htons(mPort);
    memcpy(&remote.sin_addr, mInAddr, mInAddrSize);

    configure_socket(mSocketFd);
    if (::connect(mSocketFd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) != 0) {
        const std::string error = strerror(errno);
        Close();
        throw "TcpCommunicator: Can't connect to socket: " + error + ".";
    }

#ifdef DEBUG
    printf("TcpCommunicator::Connect() returned\n");
#endif
}

void TcpCommunicator::Close() {
#ifndef _WIN32
    close_socket(mSocketFd);
#else
    if (mSocketFd >= 0) {
        shutdown(mSocketFd, SD_BOTH);
        closesocket(mSocketFd);
        mSocketFd = -1;
    }
#endif
}

size_t TcpCommunicator::Read(char *buffer, size_t size) {
#ifdef DEBUG
    printf("TcpCommunicator::Read() called\n");
#endif

    size_t received = 0;
    while (received < size) {
#ifndef _WIN32
        const ssize_t result = recv(mSocketFd, buffer + received, size - received, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0)
            throw "TcpCommunicator: Receive failed: " + std::string(strerror(errno)) + ".";
#else
        const int chunk = static_cast<int>(std::min(
            size - received, static_cast<size_t>(std::numeric_limits<int>::max())));
        const int result = recv(mSocketFd, buffer + received, chunk, 0);
        if (result == SOCKET_ERROR) throw std::string("TcpCommunicator: Receive failed.");
#endif
        if (result == 0) return received;
        received += static_cast<size_t>(result);
    }

#ifdef DEBUG
    for (unsigned int i = 0; i < size; i++) printf("%d LETTO %02X\n", i, buffer[i]);
#endif

#ifdef DEBUG
    printf("TcpCommunicator::Read() returned %zu\n", received);
#endif

    return received;
}

size_t TcpCommunicator::Write(const char *buffer, size_t size) {
#ifdef DEBUG
    printf("TcpCommunicator::Write() called\n");
#endif

    size_t sent = 0;
    while (sent < size) {
#ifndef _WIN32
#ifdef MSG_NOSIGNAL
        const ssize_t result = send(mSocketFd, buffer + sent, size - sent, MSG_NOSIGNAL);
#else
        const ssize_t result = send(mSocketFd, buffer + sent, size - sent, 0);
#endif
        if (result < 0 && errno == EINTR) continue;
        if (result < 0)
            throw "TcpCommunicator: Send failed: " + std::string(strerror(errno)) + ".";
#else
        const int chunk = static_cast<int>(std::min(
            size - sent, static_cast<size_t>(std::numeric_limits<int>::max())));
        const int result = send(mSocketFd, buffer + sent, chunk, 0);
        if (result == SOCKET_ERROR) throw std::string("TcpCommunicator: Send failed.");
#endif
        if (result == 0) throw std::string("TcpCommunicator: Send made no progress.");
        sent += static_cast<size_t>(result);
    }

#ifdef DEBUG
    for (unsigned int i = 0; i < size; i++) printf("%d SCRITTO %02X \n", i, buffer[i]);
#endif

#ifdef DEBUG
    printf("TcpCommunicator::Write() returned %zu\n", size);
#endif

    return sent;
}

void TcpCommunicator::Sync() {}

#ifndef GVIRTUS_TCP_NO_PLUGIN_FACTORY
extern "C" std::shared_ptr <TcpCommunicator> create_communicator(
        std::shared_ptr <gvirtus::communicators::Endpoint> end) {
    std::string arg =
            "tcp://" +
            std::dynamic_pointer_cast<gvirtus::communicators::Endpoint_Tcp>(end) ->address() +
            ":" +
            std::to_string(std::dynamic_pointer_cast<gvirtus::communicators::Endpoint_Tcp>(end)->port());
    return std::make_shared<TcpCommunicator>(arg);
}
#endif
