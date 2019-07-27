
#include <boost/asio.hpp>

#include <iostream>

int main()
{
    boost::asio::io_context io_ctx;

    boost::asio::ip::tcp::resolver reslv(io_ctx);
    auto const resolved = reslv.resolve("localhost", "13444");
    std::cout << "Found " << resolved.size() << " resolved endpoints.\n";

    boost::asio::ip::tcp::endpoint const endpoint = [&resolved]() {
        for (auto const& ep : resolved) {
            if (ep.endpoint().protocol() == boost::asio::ip::tcp::v4()) {
                std::cout << "Choosing " << ep.host_name() << ":" << ep.service_name() << std::endl;
                return ep.endpoint();
            }
        }
        return boost::asio::ip::tcp::endpoint();
    }();

    boost::asio::ip::tcp::socket sock(io_ctx);
    sock.connect(endpoint);
    std::cout << "Connected!";

    sock.close();
}
