#include "clsl.hpp"
#include <SFML/Graphics.hpp>

/*std::thread pt(&Server::pingHandle, &s);
std::thread ct(&Server::connectionHandle, &s, Server::stdConnectFunction);
std::thread msgHandle(&Server::msgHandle, &s);*/

int main() {
    int mode = 0;
    std::cout << "Enter mode: [0: Client, 1: Server]" << std::endl;
    std::cin >> mode;

    if (mode == 0) {
        sf::IpAddress sIP;
        std::cout << "Enter Server IP:" << std::endl;
        std::cin >> sIP;
        std::cout << std::endl;
        Client c(sIP);

        UPW data;
        std::thread t(&Client::pingHandle, &c);
        c.connectToServer(data);

        sf::UdpSocket &s = c.getSocket();

        while (true) {
            std::cout << "Enter message: [Enter exit to quit the program]" << std::endl;
            std::string msg;
            std::cin >> msg;
            if (msg == "exit") {
                break;
            }
            data.paramC = msg;
            sf::Packet packet;
            packet << msg;
            if (s.send(packet, sIP, 54003)) {
                std::cerr << "Could not send message to server" << std::endl;
            }
        }

        c.setStatusOffline();

        t.join();
    } else if (mode == 1) {
        Server s;
        std::thread pt(&Server::pingHandle, &s);
        std::thread ct(&Server::connectionHandle, &s, Server::stdConnectFunction);
        std::thread msgHandle(&Server::msgHandle, &s);

        while (true) {
            std::string cmd;
            std::cin >> cmd;
            if (cmd == "exit") {
                break;
            }

            std::lock_guard<std::mutex> l_m_msgQueueRcv(s.getMutexRcv());
            if (!s.getReqQueue().empty()) {
                for (const auto &e : s.getReqQueue()) {
                    std::cout << "[" << e.first.first << ":" << e.first.second << "] : " << e.second.paramC.toAnsiString() << std::endl;
                }
                s.getReqQueue().clear();
            }
        }

        s.setServerOffline();

        pt.join();
        ct.join();
        msgHandle.join();
    }


    return 0;
}