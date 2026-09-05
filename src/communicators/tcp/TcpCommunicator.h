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
 * @file   TcpCommunicator.h
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Thu Oct 8 12:08:33 2009
 *
 * @brief
 *
 *
 */

#pragma once

#include "gvirtus/communicators/Communicator.h"

namespace gvirtus::communicators {
/**
 * TcpCommunicator implements a Communicator for the TCP/IP socket.
 */
class TcpCommunicator : public Communicator {
 public:
  TcpCommunicator() = default;
  TcpCommunicator(const std::string &communicator);
  TcpCommunicator(const char *hostname, short port);
  TcpCommunicator(int fd, const char *hostname);
  ~TcpCommunicator() override;
  void Serve() override;
  const Communicator *Accept() const override;
  void Connect() override;
  size_t Read(char *buffer, size_t size) override;
  size_t Write(const char *buffer, size_t size) override;
  void Sync() override;
  void Close() override;

  std::string to_string() override { return "tcpcommunicator"; }

 private:
  std::string mHostname;
  char *mInAddr = nullptr;
  int mInAddrSize = 0;
  short mPort = 0;
  int mSocketFd = -1;
};
}  // namespace gvirtus::communicators
