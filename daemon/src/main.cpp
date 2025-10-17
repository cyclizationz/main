#include <iostream>
#include <unordered_set>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include "hid_report.h"
#ifdef HAVE_MSQUIC
#include "msquic_server.h"
#endif

using boost::asio::ip::udp;
static std::atomic<bool> g_running{true};

struct Args {
    std::string udp_bind = "0.0.0.0";
    uint16_t udp_port = 4444;
    std::string hid_socket = "/tmp/hidra.kbd";
    int watchdog_ms = 200;
    int deadline_ms = 100;
    bool verbose = false;
};

static void parse_args(int argc, char** argv, Args& a) {
    for (int i=1; i<argc; ++i) {
        std::string s = argv[i];
        auto next = [&]{ return (i+1<argc) ? std::string(argv[++i]) : std::string(); };
        if (s == "--udp-edges") {
            auto v = next();
            auto pos = v.find(':');
            if (pos != std::string::npos) {
                a.udp_bind = v.substr(0,pos);
                a.udp_port = static_cast<uint16_t>(std::stoi(v.substr(pos+1)));
            }
        } else if (s == "--hid-socket") {
            a.hid_socket = next();
        } else if (s == "--watchdog-ms") {
            a.watchdog_ms = std::stoi(next());
        } else if (s == "--deadline-ms") {
            a.deadline_ms = std::stoi(next());
        }
        else if (s == "--verbose" || s == "-v")
        {
            a.verbose = true;
        }
    }
}

class HidSocket
{
public:
    explicit HidSocket(const std::string& path, bool verbose = false) : path_(path), verbose_(verbose)
    {
    }

    ~HidSocket() { if (fd_ >= 0) close(fd_); }

    bool ensure_connected()
    {
        if (fd_ >= 0) return true;
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path_.c_str());
        if (::connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            if (verbose_)
                std::cerr << "[hidra] HID socket connect failed (" << path_
                    << "). Waiting for QEMU to listen... (set HID_SOCK or run scripts/run_qemu.sh)\n";
            close(fd_);
            fd_ = -1;
            return false;
        }
        if (verbose_) std::cerr << "[hidra] Connected to HID socket: " << path_ << "\n";
        return true;
    }

    void send_report(const HidReport8& r)
    {
        if (fd_ < 0 && !ensure_connected()) return;
        ssize_t n = ::send(fd_, r.bytes.data(), r.bytes.size(), MSG_NOSIGNAL);
        if (n < 0)
        {
            if (verbose_) std::cerr << "[hidra] send() failed; will retry connect\n";
            close(fd_);
            fd_ = -1;
        }
    }

private:
    std::string path_;
    int fd_ = -1;
    bool verbose_ = false;
};

int main(int argc, char** argv) {
    Args args;
    parse_args(argc, argv, args);

    signal(SIGINT, [](int) { g_running = false; });
    signal(SIGTERM, [](int) { g_running = false; });

    if (args.verbose)
    {
        std::cerr << "[hidra] daemon starting\n"
            << "  udp edges : " << args.udp_bind << ":" << args.udp_port << "\n"
            << "  hid socket: " << args.hid_socket << " (QEMU should be server=on)\n"
            << "  watchdog  : " << args.watchdog_ms << " ms\n"
            << "  deadline  : " << args.deadline_ms << " ms\n";
    }

    HidSocket hid(args.hid_socket, args.verbose);

    std::unordered_set<uint8_t> pressed;
    uint8_t modifiers = 0x00;

    boost::asio::io_context io;
    udp::socket sock(io, udp::endpoint(boost::asio::ip::make_address(args.udp_bind), args.udp_port));
    std::array<char, 1024> buf{};
    udp::endpoint sender;

    uint64_t cnt_rx = 0, cnt_emit = 0;
    auto last_beat = std::chrono::steady_clock::now();

#ifdef HAVE_MSQUIC
    MsQuicServer quicServer;
    quicServer.start("0.0.0.0", 4445, "hidra-snp",
        [&](const uint8_t* data, size_t len, uint64_t, uint32_t){
            // TODO: parse protobuf Snapshot, update 'pressed' + 'modifiers', emit HID
            std::vector<uint8_t> v; v.reserve(6);
            for (auto u : pressed) { if (v.size() < 6) v.push_back(u); }
            auto report = build_hid_report(v, modifiers);
            hid.send_report(report);
            ++cnt_emit;
                     });
    if (args.verbose) std::cerr << "[hidra] msquic snapshot listener on :4445 (ALPN hidra-snp)\n";
#endif

    while (g_running) {
        boost::system::error_code ec;
        size_t n = sock.receive_from(boost::asio::buffer(buf), sender, 0, ec);
        if (ec) continue;
        std::string s(buf.data(), n);
        std::string cmd, key;
        auto sp = s.find(' ');
        if (sp != std::string::npos) {
            cmd = s.substr(0, sp);
            key = s.substr(sp+1);
            while (!key.empty() && (key.back()=='\n' || key.back()=='\r' || key.back()==' ')) key.pop_back();
        }
        auto mapChar = [](char c)->uint8_t{
            if (c>='a'&&c<='z') return 0x04 + (c-'a');
            if (c=='\n') return 0x28;
            return 0;
        };
        uint8_t usage = 0;
        if (key == "ENTER") usage = 0x28;
        else if (!key.empty()) usage = mapChar(tolower(key[0]));

        if (usage) {
            if (cmd == "down") pressed.insert(usage);
            else if (cmd == "up") pressed.erase(usage);
            std::vector<uint8_t> v; v.reserve(6);
            for (auto u : pressed) { if (v.size() < 6) v.push_back(u); }
            auto report = build_hid_report(v, modifiers);
            hid.send_report(report);
            ++cnt_emit;
        }
        ++cnt_rx;

        auto now = std::chrono::steady_clock::now();
        if (args.verbose && std::chrono::duration_cast<std::chrono::seconds>(now - last_beat).count() >= 2)
        {
            std::cerr << "[hidra] progress: rx=" << cnt_rx << " emits=" << cnt_emit
                << " socket=" << (hid.ensure_connected() ? "connected" : "waiting") << "\n";
            last_beat = now;
        }
    }

    if (args.verbose)
    {
        std::cerr << "[hidra] shutting down. totals: rx=" << cnt_rx << " emits=" << cnt_emit << "\n";
    }
    return 0;
}
