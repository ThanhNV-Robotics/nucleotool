#pragma once

#include <string>
#include <utility>
#include <vector>

#include <mab_types.hpp>
#include <logger.hpp>
#include <I_communication_interface.hpp>

namespace mab
{
    /// @brief CANdle commands tunneled over UDP to the NUCLEO-H755 leg bridge (FDCanUdp project).
    ///
    /// The board emulates one CANdle per FDCAN bus (L = FDCAN1, R = FDCAN2). Each transfer is one
    /// datagram each way, with a 4-byte tunnel header in front of the raw CANdle bytes:
    ///   request: [0xCD][bus 0/1][seq u16 LE][CANdle command ...]
    ///   reply:   [0xCD][status ][seq u16 LE][CANdle reply ...]
    /// Replies whose seq does not match the request (late answers to a timed-out request) are
    /// dropped. The board only accepts tool requests while it is in MAINTENANCE mode.
    class UDP final : public I_CommunicationInterface
    {
      public:
        static constexpr u8  TUNNEL_MAGIC  = 0xCD;
        static constexpr u16 DEFAULT_PORT  = 9998;
        static constexpr u32 NET_MARGIN_MS = 50;  // added to every CANdle timeout

        enum TunnelStatus_E : u8
        {
            ST_OK               = 0,
            ST_NOT_MAINTENANCE  = 1,  // board runs the control loop, tool locked out
            ST_BUSY             = 2,  // another tool request is still being processed
            ST_BAD_REQUEST      = 3,
            ST_UNSUPPORTED      = 4,  // CANdle command the emulation does not implement
            ST_DATARATE         = 5,  // requested datarate differs from the bus setting
        };

        /// @param device "L", "R", "<ip>:L" or "<ip>:R" (default ip 192.168.10.10)
        explicit UDP(const std::string& device = "");
        ~UDP();

        Error_t connect() override;
        Error_t disconnect() override;
        Error_t transfer(std::vector<u8> data, const u32 timeoutMs) override;
        std::pair<std::vector<u8>, Error_t> transfer(std::vector<u8> data,
                                                     const u32       timeoutMs,
                                                     const size_t expectedReceivedDataSize) override;

      private:
        Logger      m_log = Logger(Logger::ProgramLayer_E::BOTTOM, "UDP");
        std::string m_ip  = "192.168.10.10";
        u16         m_port = DEFAULT_PORT;
        u8          m_bus  = 0;
        int         m_sock = -1;
        u16         m_seq  = 0;
    };
}  // namespace mab
