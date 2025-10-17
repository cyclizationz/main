#include <iostream>
#include <unordered_set>
#include <utility>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>
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
    std::string hid_socket = "/run/hid.kbd";
    int watchdog_ms = 200;
    int deadline_ms = 100;
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
    }
}

class HidSocket {
public:
    explicit HidSocket(std::string path) : path_(std::move(path)) { connect(); }
    ~HidSocket() { if (fd_>=0) close(fd_); }
    void send_report(const HidReport8& r) {
        if (fd_ < 0) { connect(); }
        if (fd_ >= 0) {
            ::send(fd_, r.bytes.data(), r.bytes.size(), MSG_NOSIGNAL);
        }
    }
private:
    void connect() {
        if (fd_>=0) { close(fd_); fd_=-1; }
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path_.c_str());
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            close(fd_); fd_ = -1;
        }
    }
    std::string path_;
    int fd_ = -1;
};

int main(int argc, char** argv) {
    Args args;
    parse_args(argc, argv, args);

    signal(SIGINT, [](int){ g_running=false; });
    signal(SIGTERM, [](int){ g_running=false; });

    HidSocket hid(args.hid_socket);

    // Authoritative pressed set (demo). In full build, maintain both working and authoritative sets.
    std::unordered_set<uint8_t> pressed;
    uint8_t modifiers = 0x00;

    // UDP edges (quick path)
    boost::asio::io_context io;
    udp::socket sock(io, udp::endpoint(boost::asio::ip::make_address(args.udp_bind), args.udp_port));
    std::array<char,1024> buf{};
    udp::endpoint sender;

#ifdef HAVE_MSQUIC
    MsQuicServer quicServer;
    quicServer.start("0.0.0.0", 4445, "hidra-snp",
        [&](const uint8_t* data, size_t len, uint64_t, uint32_t){
            // TODO: parse protobuf Snapshot, update 'pressed' + 'modifiers', emit HID
            std::vector<uint8_t> v; v.reserve(6);
            for (auto u : pressed) { if (v.size() < 6) v.push_back(u); }
            auto report = build_hid_report(v, modifiers);
            hid.send_report(report);
        });
#endif

    while (g_running) {
        boost::system::error_code ec;
        size_t n = sock.receive_from(boost::asio::buffer(buf), sender, 0, ec);
        if (ec) continue;
        std::string s(buf.data(), n);
        // Very simple demo protocol: "down a" or "up a"
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
        }
    }
    return 0;
}
