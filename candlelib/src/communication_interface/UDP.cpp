#include "UDP.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

namespace mab
{
    static const char* statusText(u8 status)
    {
        switch (status)
        {
            case UDP::ST_NOT_MAINTENANCE:
                return "board is in CONTROL mode, tool requests are locked out";
            case UDP::ST_BUSY:
                return "board is still busy with a previous tool request";
            case UDP::ST_BAD_REQUEST:
                return "board rejected a malformed request";
            case UDP::ST_UNSUPPORTED:
                return "CANdle command not supported by the board";
            case UDP::ST_DATARATE:
                return "datarate not supported: the legs run at 8M, use -d 8M";
            default:
                return "unknown tunnel status";
        }
    }

    UDP::UDP(const std::string& device)
    {
        std::string leg = device;
        auto        sep = device.rfind(':');
        if (sep != std::string::npos)
        {
            m_ip = device.substr(0, sep);
            leg  = device.substr(sep + 1);
        }
        if (leg == "R" || leg == "r" || leg == "1")
            m_bus = 1;
        else if (!(leg.empty() || leg == "L" || leg == "l" || leg == "0"))
            m_log.warn("Unknown leg '%s', using L (FDCAN1)", leg.c_str());
    }

    UDP::~UDP()
    {
        disconnect();
    }

    UDP::Error_t UDP::connect()
    {
        disconnect();
        m_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sock < 0)
        {
            m_log.error("Could not open UDP socket: %s", strerror(errno));
            return Error_t::INITIALIZATION_ERROR;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(m_port);
        if (inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr) != 1)
        {
            m_log.error("Invalid board address '%s'", m_ip.c_str());
            disconnect();
            return Error_t::INITIALIZATION_ERROR;
        }
        // A connected UDP socket only receives datagrams from the board
        if (::connect(m_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            m_log.error("Could not reach %s: %s", m_ip.c_str(), strerror(errno));
            disconnect();
            return Error_t::NOT_CONNECTED;
        }
        m_seq = (u16)std::chrono::steady_clock::now().time_since_epoch().count();
        m_log.info("Using board %s:%u, leg %s", m_ip.c_str(), m_port, m_bus ? "R" : "L");
        return Error_t::OK;
    }

    UDP::Error_t UDP::disconnect()
    {
        if (m_sock < 0)
            return Error_t::NOT_CONNECTED;
        close(m_sock);
        m_sock = -1;
        return Error_t::OK;
    }

    UDP::Error_t UDP::transfer(std::vector<u8> data, const u32 timeoutMs)
    {
        return transfer(data, timeoutMs, 0).second;
    }

    std::pair<std::vector<u8>, UDP::Error_t> UDP::transfer(std::vector<u8> data,
                                                           const u32       timeoutMs,
                                                           const size_t    expectedReceivedDataSize)
    {
        if (m_sock < 0)
        {
            m_log.error("Device not connected!");
            return std::pair(data, Error_t::NOT_CONNECTED);
        }
        if (data.empty())
            return std::pair(data, Error_t::DATA_EMPTY);
        if (data.size() > 512)
            return std::pair(data, Error_t::DATA_TOO_LONG);

        const u16       seq = ++m_seq;
        std::vector<u8> packet = {TUNNEL_MAGIC, m_bus, (u8)seq, (u8)(seq >> 8)};
        packet.insert(packet.end(), data.begin(), data.end());
        if (send(m_sock, packet.data(), packet.size(), 0) != (ssize_t)packet.size())
        {
            m_log.error("UDP send failed: %s", strerror(errno));
            return std::pair(data, Error_t::TRANSMITTER_ERROR);
        }
        if (expectedReceivedDataSize == 0)
            return std::pair(data, Error_t::OK);

        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs + NET_MARGIN_MS);
        u8 rx[1024];
        while (true)
        {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now())
                            .count();
            if (left < 0)
                left = 0;
            pollfd pfd{m_sock, POLLIN, 0};
            int    ready = poll(&pfd, 1, (int)left);
            if (ready == 0)
            {
                m_log.error("No reply from the board (timeout %u ms)", timeoutMs + NET_MARGIN_MS);
                return std::pair(data, Error_t::TIMEOUT);
            }
            if (ready < 0)
            {
                if (errno == EINTR)
                    continue;
                m_log.error("poll failed: %s", strerror(errno));
                return std::pair(data, Error_t::RECEIVER_ERROR);
            }
            ssize_t n = recv(m_sock, rx, sizeof(rx), 0);
            if (n < 0)
            {
                // ECONNREFUSED: ICMP port unreachable, nothing listens on the board
                m_log.error("UDP receive failed: %s", strerror(errno));
                return std::pair(data, Error_t::NOT_CONNECTED);
            }
            if (n < 4 || rx[0] != TUNNEL_MAGIC || (u16)(rx[2] | (rx[3] << 8)) != seq)
            {
                m_log.debug("Dropping stale or foreign datagram (%d bytes)", (int)n);
                continue;
            }
            if (rx[1] != ST_OK)
            {
                m_log.error("%s", statusText(rx[1]));
                return std::pair(data,
                                 rx[1] == ST_UNSUPPORTED || rx[1] == ST_DATARATE
                                     ? Error_t::NOT_SUPPORTED
                                     : Error_t::NOT_CONNECTED);
            }
            std::vector<u8> reply(rx + 4, rx + n);
            reply.resize(expectedReceivedDataSize, 0);
            return std::pair(reply, Error_t::OK);
        }
    }
}  // namespace mab
