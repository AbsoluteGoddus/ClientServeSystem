//
// Created by absol on 7/16/2024.
//

#ifndef CLIENTSERVERSYSTEM_CLSL_HPP
#define CLIENTSERVERSYSTEM_CLSL_HPP

#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>
#include <functional>
#include <SFML/Network.hpp>

using namespace std::chrono_literals;

// Universal Package Wrapper
struct UPW {
    sf::Uint64 type = 0; // The type of the package.
    sf::Uint64 paramA = 0;
    sf::Uint64 paramB = 0;
    sf::String paramC = "";
    sf::String paramD = "";
};

sf::Packet& operator << (sf::Packet& packet, const UPW& package) {
    return packet << package.type << package.paramA << package.paramB << package.paramC << package.paramD;
}

sf::Packet& operator >> (sf::Packet& packet, UPW& package) {
    return packet >> package.type >> package.paramA >> package.paramB >> package.paramC >> package.paramD;
}

class Server {
private:
    sf::UdpSocket connectionSocket;
    sf::UdpSocket pingSocket;
    sf::UdpSocket socket;

    std::vector<sf::IpAddress> clients = {};
    std::mutex m_clients = {};

    std::vector<std::pair<sf::IpAddress, UPW>> msgQueueSnd = {};
    std::mutex m_msgQueueSnd = {};

    std::vector<std::pair<std::pair<sf::IpAddress, unsigned short>, UPW>> msgQueueRcv = {};
    std::mutex m_msgQueueRcv = {};

    bool serverOnline = false;


public:

    static bool stdConnectFunction(Server& server, UPW &req, sf::IpAddress &IP) {
        std::cout << "[Connection Req]: Client [" << IP.toString() << "] accepted" << std::endl;
        UPW package;
        package.type = 4; // Connection Accepted

        server.pushMsg(IP, package);

        return true;
    };

    Server() {
        if (connectionSocket.bind(54000) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54000");
        }

        if (pingSocket.bind(54001) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54001");
        }

        if (socket.bind(54002) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54002");
        }

        serverOnline = true;
    }

    Server(const unsigned short cs, const unsigned short ps, const unsigned short s) {
        if (connectionSocket.bind(cs) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: " + std::to_string(cs));
        }

        if (pingSocket.bind(ps) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: " + std::to_string(ps));
        }

        if (socket.bind(s) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: " + std::to_string(s));
        }

        serverOnline = true;
    }

    void pushMsg(sf::IpAddress to, const UPW& package) {
        std::lock_guard<std::mutex> l_m_msgQueueSnd(m_msgQueueSnd);
        msgQueueSnd.emplace_back(to, package);
    }

    // ALWAYS, NO MATTER WHAT USE A LOCK_GUARD
    std::vector<std::pair<std::pair<sf::IpAddress, unsigned short>, UPW>> &getReqQueue() {
        return msgQueueRcv;
    };

    std::mutex &getMutexRcv() {
        return m_msgQueueRcv;
    }

    void pushClient(sf::IpAddress c) {
        std::lock_guard<std::mutex> l_m_clients(m_clients);
        clients.push_back(c);
    }

    void setServerOffline() {
        serverOnline = false;
    }

    // Sub-Function-Threads for msgHandle
    void msgHandleSnd() {
        while (serverOnline) {
            std::this_thread::sleep_for(100ms);
            std::lock_guard<std::mutex> l_m_msgQueueSnd(m_msgQueueSnd);
            if (!msgQueueSnd.empty()) {
                for (const auto& package : msgQueueSnd) {
                    sf::Packet packet;
                    packet << package.second;
                    if (socket.send(packet, package.first, 54000) != sf::Socket::Done) {
                        std::cerr << "Error sending package to Client [" << package.first.toString() << "]" << std::endl;
                    }
                }
                msgQueueSnd.clear();
            }
        }
    }

    void msgHandleRcv() {
        while (serverOnline) {
            std::this_thread::sleep_for(10ms);
            sf::IpAddress sender;
            unsigned short senderPort;
            sf::Packet packet;
            UPW package;
            if (socket.receive(packet, sender, senderPort) == sf::Socket::Done) {
                if (std::find(clients.begin(), clients.end(), sender) != clients.end()) {
                    std::cout << "Received Packet from Client [" << sender << "]" << std::endl;
                    std::lock_guard<std::mutex> l_m_msgQueueRcv(m_msgQueueRcv);
                    packet >> package;
                    msgQueueRcv.push_back({{sender, senderPort}, package});
                }
            }
        }
    }

    // Gets and sends messages.
    // Received messages are put into msgQueueRcv
    // Messages to be sent are pulled from msgQueueSnd
    void msgHandle() {
        socket.setBlocking(false);
        std::thread t_msgHandleSnd(&Server::msgHandleSnd, &*this);
        std::thread t_msgHandleRcv(&Server::msgHandleRcv, &*this);

        t_msgHandleRcv.join();
        t_msgHandleSnd.join();
    }

    // Accepts a connection request using the function parameter
    void connectionHandle(const std::function<bool(Server&, UPW&, sf::IpAddress&)>& function = stdConnectFunction) {
        connectionSocket.setBlocking(false);
        while (serverOnline) {
            std::this_thread::sleep_for(1s);
            sf::Packet data;
            sf::IpAddress sender;
            unsigned short senderPort;
            UPW req;
            if (connectionSocket.receive(data, sender, senderPort) == sf::Socket::Done) {
                data >> req;
                if (function(*this, req, sender)) {
                    std::lock_guard<std::mutex> l_m_clients(m_clients);
                    clients.push_back(sender);
                }
            }
        }
    }

    // Sends the data directly without it waiting 100ms after every iteration
    bool liveSend(sf::IpAddress to, const UPW& package) {
        sf::Packet packet;
        packet << package;
        if (socket.send(packet, to, 54000) != sf::Socket::Done) {
            std::cerr << "Could not send livePackage to Client [" << to.toString() << "]" << std::endl;
            return false;
        }
        return true;
    }

    void pingHandle() {
        pingSocket.setBlocking(false);

        while (serverOnline) {
            std::this_thread::sleep_for(10s);
            std::vector<sf::IpAddress> unresponsiveClients;

            for (auto IP : clients) {
                UPW req;
                req.type = 0; // ping
                sf::Packet packet;
                packet << req;
                bool success = false;
                if (pingSocket.send(packet, IP, 54001) != sf::Socket::Done) {  // Use the correct port
                    std::cerr << "Unable to ping: " << IP.toString() << std::endl;
                } else {
                    sf::Clock c;
                    sf::IpAddress sender;
                    unsigned short senderPort;
                    UPW res;
                    while (c.getElapsedTime() <= sf::seconds(10)) {
                        if (pingSocket.receive(packet, sender, senderPort) == sf::Socket::Done) {
                            if (sender == IP) {
                                packet >> res;
                                if (res.type == 1) { // pong
                                    success = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!success) {
                    std::cerr << "[Ping Tst]: Client [" << IP.toString() << "] does not respond" << std::endl;
                    unresponsiveClients.push_back(IP);
                }
            }

            std::lock_guard<std::mutex> l_m_clients(m_clients);
            for (auto IP : unresponsiveClients) {
                clients.erase(std::remove(clients.begin(), clients.end(), IP), clients.end());
                std::cerr << "[Ping Tst]: Removed Client [" << IP << "]" << std::endl;
            }
        }
    }
};

// A simple client class. Makes your and my life easier
class Client {
private:
    sf::UdpSocket pingSocket;
    sf::UdpSocket socket;
    sf::IpAddress sIP;
    unsigned short s_connectionSocket;
    unsigned short s_pingSocket;

    bool status = false;

public:
    Client(sf::IpAddress serverIP) {
        sIP = serverIP;
        s_connectionSocket = 54000;
        s_pingSocket = 54001;

        if (socket.bind(54000) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54000");
        }

        if (pingSocket.bind(54001) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54001");
        }

        status = true;
    }

    Client(sf::IpAddress serverIP, unsigned short arg_s_connectionPort, unsigned short arg_s_pingPort) {
        sIP = serverIP;
        s_connectionSocket = arg_s_connectionPort;
        s_pingSocket = arg_s_pingPort;

        if (socket.bind(54000) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54000");
        }

        if (pingSocket.bind(54001) != sf::Socket::Done) {
            throw std::runtime_error("Unable to bind to UdpSocket at: 54001");
        }

        status = true;
    }

    bool connectToServer(UPW &req) {
        sf::Packet packet;
        packet << req;
        if (socket.send(packet, sIP, s_connectionSocket) != sf::Socket::Done) {
            std::cerr << "Unable to send connection request!" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(1s);
        UPW data;
        sf::Packet res;
        sf::IpAddress sender;
        unsigned short senderPort;

        if (socket.receive(res, sender, senderPort) != sf::Socket::Done) {
            std::cerr << "Unable to read connection answer" << std::endl;
        }

        res >> data;

        if (data.type == 4) {
            std::cout << "Successfully connected to server" << std::endl;
            return true;
        }

        std::cerr << "Connection request denied" << std::endl;
        return false;
    }

    void pingHandle() {
        pingSocket.setBlocking(false);
        while (status) {
            std::this_thread::sleep_for(100ms);
            sf::Packet packet;
            UPW package;
            sf::IpAddress sender;
            unsigned short senderPort;
            if (pingSocket.receive(packet, sender, senderPort) == sf::Socket::Done) {
                packet >> package;
                if (package.type == 0) { // ping
                    package.type = 1; // pong
                    sf::Packet res;
                    res << package;
                    if (pingSocket.send(res, sIP, s_pingSocket) != sf::Socket::Done) {
                        std::cerr << "Unable to send ping" << std::endl;
                    }
                }
            }
        }
    }

    void setStatusOffline() {
        status = false;
    }

    sf::UdpSocket& getSocket() {
        return socket;
    }
};

#endif //CLIENTSERVERSYSTEM_CLSL_HPP
